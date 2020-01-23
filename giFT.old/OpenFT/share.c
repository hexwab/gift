/*
 * share.c
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "openft.h"

#include "netorg.h"
#include "queue.h"
#include "search.h"

#include "http.h"
#include "html.h"

#include "parse.h"
#include "file.h"

#include "share.h"

/*****************************************************************************/

/**/extern Connection *ft_self;
/**/extern Config     *openft_conf;

/*
 * for those who know perl, this will help explain the data structure
 * organization:
 *
 * my %ft_shares =
 * (
 *   65.4.102.175  => [ MD51 => FileShare data1,
 *                      MD52 => FileShare data2,
 *                      MD53 => FileShare data3 ],
 *   66.189.160.12 => [ MD51 => FileShare data1,
 *                      MD52 => FilEShare data2 ],
 * );
 *
 * organization used to quickly remove/locate shares by user
 *
 */
HashTable *ft_shares = NULL;

/*****************************************************************************/

int openft_share_complete (FileShare *file)
{
	OpenFT_Share *openft;
	int           ret = FALSE;

	if (!file)
		return FALSE;

	openft = dataset_lookup (file->data, "OpenFT");

	if (file->path && file->md5 && file->size)
	{
		if (file->path[0] != '/')
			return FALSE;

		if (!openft->tokens)
		{
			/* tokenize this query for fast searching */
			openft->tokens = search_tokenize (file->path);
		}

		if (openft->tokens)
			ret = TRUE;
	}

	return ret;
}

/*****************************************************************************/

static void openft_share_free_proto (FileShare *file)
{
	OpenFT_Share *openft;

	if (!file)
		return;

	openft = dataset_lookup (file->data, "OpenFT");

	/* handle the ref counting */
	openft->destroying = TRUE;

	/* it's not safe to free this now...something is still using it */
	if (openft->ref)
	{
		TRACE (("unable to free %p, ref count = %i", file, openft->ref));
		return;
	}

	free (openft->tokens);
	free (openft);

	dataset_remove (file->data, "OpenFT");
}

void openft_share_free (FileShare *file)
{
	openft_share_free_proto (file);

	share_free (file);
}

/*****************************************************************************/

FileShare *openft_share_new (char *host, unsigned short port,
                             unsigned short http_port, unsigned long size,
                             char *md5, char *filename)
{
	FileShare    *file;
	OpenFT_Share *openft;

	file = share_new (NULL, filename, md5, size, 0);

	openft = malloc (sizeof (OpenFT_Share));
	memset (openft, 0, sizeof (OpenFT_Share));

	openft->destroying = FALSE;
	openft->host       = inet_addr (host);
	openft->port       = port;
	openft->http_port  = http_port;

	openft->tokens = NULL;

	dataset_insert (file->data, "OpenFT", openft);

	openft_share_ref (file);

	return file;
}

/*****************************************************************************/

static int openft_share_ref_mod (FileShare *file, int val)
{
	OpenFT_Share *openft;

	openft = dataset_lookup (file->data, "OpenFT");
	openft->ref += val;

	if (openft->ref < 0)
	{
		GIFT_WARN (("file %p has negative ref count!", file));
	}

	return openft->ref;
}

void openft_share_ref (FileShare *file)
{
	openft_share_ref_mod (file, 1);
}

void openft_share_unref (FileShare *file)
{
	int ref;

	ref = openft_share_ref_mod (file, -1);

	if (!ref)
		openft_share_free (file);
}

/*****************************************************************************/

void openft_share_add (FileShare *file)
{
	OpenFT_Share *openft;
	Dataset      *host_table;

	/* verify that this is a complete share */
	if (!openft_share_complete (file))
	{
		openft_share_unref (file);
		return;
	}

	/* we need host */
	openft = dataset_lookup (file->data, "OpenFT");

	/* create the table if needed */
	if (!ft_shares)
		ft_shares = hash_table_new ();

	/* locate this hosts insertion table */
	if (!(host_table = hash_table_lookup (ft_shares, openft->host)))
	{
		/* first share from this host, build our table */
		host_table = dataset_new ();
		hash_table_insert (ft_shares, openft->host, host_table);
	}

	/* insert the fileshare */
	dataset_insert (host_table, file->md5, file);

	/* stats gathering */
	openft_share_stats_add (ft_self, file);
}

#if 0
void openft_share_remove (FileShare *file)
{
	ft_shares = list_remove (ft_shares, file);
	openft_share_stats_remove (ft_self, file);

	openft_share_unref (file);
}
#endif

/*****************************************************************************/

static int destroy_host_table (unsigned long key, void *value, void *udata)
{
	/* ref is most likely at one right now, if not, oh well, it'll be freed
	 * later */
	openft_share_unref (value);

	return TRUE;
}

void openft_share_remove_by_host (unsigned long host)
{
	Dataset *host_table;

	if (!host)
		return;

	/* user is not sharing any files, no big deal */
	if (!(host_table = hash_table_lookup (ft_shares, host)))
		return;

	TRACE (("%s", net_ip_str (host)));

	hash_table_remove (ft_shares, host);

	/* free all the data found in host_table */
	hash_table_foreach_remove (host_table, (HashFunc) destroy_host_table, NULL);
}

/*****************************************************************************/
/* local shares (wrapped src/sharing.c) */

static int local_import (unsigned long key, FileShare *file, void *udata)
{
	OpenFT_Share *openft;

	if (dataset_lookup (file->data, "OpenFT"))
		return TRUE;

	openft = malloc (sizeof (OpenFT_Share));
	memset (openft, 0, sizeof (OpenFT_Share));

	openft->port      = NODE (ft_self)->port;
	openft->http_port = NODE (ft_self)->http_port;

	dataset_insert (file->data, "OpenFT", openft);

	/* ref this to keep it alive */
	openft_share_ref (file);

	return TRUE;
}

void openft_share_local_import ()
{
	share_foreach ((HashFunc) local_import, NULL);
}

/*****************************************************************************/

static int local_cleanup (unsigned long key, FileShare *file, void *udata)
{
	OpenFT_Share *openft;

	/* we are being unloaded, we _need_ to free this data right now */
	openft = dataset_lookup (file->data, "OpenFT");
	openft->ref = 0;

	/* get rid of the extra data */
	openft_share_free_proto (file);

	return TRUE;
}

void openft_share_local_cleanup ()
{
	share_foreach ((HashFunc) local_cleanup, NULL);
}

/*****************************************************************************/

static int local_find_node (unsigned long key, FileShare *file,
                            char *filename)
{
	if (!filename)
		return FALSE;

	if (!STRCMP (filename, file->hpath))
		return TRUE;

	return FALSE;
}

/* for security purposes, verify the supplied filename is being shared */
char *openft_share_local_verify (char *filename)
{
	FileShare *file;

	if (!filename)
		return FALSE;

	/* anything coming from the data dir is fair game */
	if (!strncmp (filename, DATA_DIR, strlen (DATA_DIR)))
		return filename;

	file = hash_table_find (share_index (NULL, NULL),
	                        (HashFunc) local_find_node, filename);

	return (file ? file->path : NULL);
}

FileShare *openft_share_local_find (char *filename)
{
	FileShare *file;

	if (!filename)
		return FALSE;

	file = hash_table_find (share_index (NULL, NULL),
	                        (HashFunc) local_find_node, filename);

	return file;
}

/*****************************************************************************/

static void local_submit_write (Connection *c, FileShare *file, void *udata)
{
	if (!file)
		return;

	ft_packet_send (c, FT_SHARE_REQUEST, "%lu%s%s",
	              file->size, file->md5, file->hpath);
}

static void local_submit_destroy ()
{
}

static Connection *locate_future_parent (Connection *c, Node *node,
                                         void *udata)
{
	if (node->class & NODE_PARENT)
		return NULL;

	return c;
}

void openft_share_local_submit (Connection *c)
{
	HashTable *shares;

	if (!(shares = share_index (NULL, NULL)))
		return;

	if (!c)
	{
		c = conn_foreach ((ConnForeachFunc) locate_future_parent, NULL,
						  NODE_SEARCH, NODE_CONNECTED, 0);

		if (!c)
		{
			TRACE (("no potential parent found"));
			return;
		}
	}

	/* TODO - hash_flatten is messy :/ */
	queue_add (c,
	           (QueueWriteFunc) local_submit_write,
	           (QueueWriteFunc) local_submit_destroy,
	           hash_flatten (shares), NULL);
}

/*****************************************************************************/

void openft_share_stats_reset (Connection *c)
{
	NODE (c)->shares = 0;
	NODE (c)->megs   = 0;
}

static double openft_share_stats_size (FileShare *file)
{
	unsigned long megs;

	megs = (((float)file->size / 1024.0) / 1024.0);

	return megs;
}

void openft_share_stats_add (Connection *c, FileShare *file)
{
	NODE (c)->shares++;
	NODE (c)->megs += openft_share_stats_size (file);
}

void openft_share_stats_remove (Connection *c, FileShare *file)
{
	NODE (c)->shares--;
	NODE (c)->megs -= openft_share_stats_size (file);
}

void openft_share_stats_set (Connection *c, unsigned long users,
                             unsigned long shares, unsigned long megs)
{
	if (users)
		NODE (c)->users = users;

	if (shares)
		NODE (c)->shares = shares;

	if (megs)
		NODE (c)->megs = megs;
}
