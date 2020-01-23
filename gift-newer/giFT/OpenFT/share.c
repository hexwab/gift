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
#include "share_comp.h"

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
 *                      MD52 => FileShare data2 ],
 * );
 *
 * organization used to quickly remove/locate shares by user
 *
 */
HashTable *ft_shares = NULL;

/* holds stats info for INDEX nodes */
static HashTable *ft_stats = NULL;

/*****************************************************************************/

int ft_share_complete (FileShare *file)
{
	FT_Share *share;
	int       ret = FALSE;

	if (!file || !file->sdata)
		return FALSE;

	if (!(share = share_lookup_data (file, "OpenFT")))
		return FALSE;

	if (file->sdata->path && file->sdata->md5 && file->size)
	{
		/* if the filename doesnt contain a leading / then return false
		 * rossta - what the hell is the ':' stuff? */
		if (file->sdata->path[0] != '/' &&
			strlen (file->sdata->path) > 1 && file->sdata->path[1] != ':')
			return FALSE;

		/* tokenize this query for fast searching */
		if (!share->tokens)
			share->tokens = search_tokenize (file->sdata->path);

		if (share->tokens)
			ret = TRUE;
	}

	return ret;
}

/*****************************************************************************/

static FT_Share *ft_share_new_proto (FileShare *file, FT_HostShare *h_share)
{
	FT_Share *share;

	if (!file || !h_share)
		return NULL;

	share = malloc (sizeof (FT_Share));

	share->host_share = h_share;
	share->tokens     = NULL;
	share->ref        = 0;

	share_insert_data (file, "OpenFT", share);

	ft_share_ref (file);

	return share;
}

/* this function is used directly only when you need to create a FileShare
 * case w/o actually adding it to ft_shares.  See ft_share_add for correct
 * usage otherwise */
FileShare *ft_share_new (FT_HostShare *h_share, unsigned long size,
                         char *md5, char *filename)
{
	FileShare *file;

	file = share_new (NULL, 0, filename, md5, size, 0);
	ft_share_new_proto (file, h_share);

	return file;
}

/*****************************************************************************/

/* HostShare structures hold info regarding all files shared by a host so that
 * each individual OpenFT Share does not have to waste the memory */
FT_HostShare *ft_host_share_new (int verified, unsigned long host,
                                 unsigned short port, unsigned short http_port)
{
	FT_HostShare *h_share;

	h_share = malloc (sizeof (FT_HostShare));

	h_share->host         = host;
	h_share->port         = port;
	h_share->http_port    = http_port;
	h_share->disabled     = FALSE;
	h_share->uploads      = 0;
	h_share->max_uploads  = -1;
	h_share->availability = 1;  /* if max_uploads is unlimited, ... */
	h_share->verified     = verified;
	h_share->dataset      = dataset_new ();

	h_share->db_timeout   = 0;
	h_share->db_flushed   = FALSE;

	return h_share;
}

FT_HostShare *ft_host_share_add (int verified, unsigned long host,
                                 unsigned short port,
                                 unsigned short http_port)
{
	FT_HostShare *h_share;

	/* create the table if needed */
	if (!ft_shares)
		ft_shares = hash_table_new ();

	/* locate this hosts insertion table */
	if (!(h_share = hash_table_lookup (ft_shares, host)))
	{
		/* first share from this host, build our table */
		h_share = ft_host_share_new (verified, host, port, http_port);
		ft_host_share_new_db (h_share);

		hash_table_insert (ft_shares, host, h_share);
	}

	return h_share;
}

void ft_host_share_free (FT_HostShare *h_share)
{
	timer_remove (h_share->db_timeout);

	dataset_clear (h_share->dataset);

	free (h_share);
}

/*****************************************************************************/

static void calc_avail (FT_HostShare *h_share)
{
	int availability = 0;

	/* figure out how many available queue slots this host has */
	if (!h_share->disabled)
	{
		/* if they are not limiting uploads, report 1 always :) */
		if (h_share->max_uploads == -1)
			availability = 1;
		else
		{
			availability =
				h_share->max_uploads -
				h_share->uploads;

			availability = MAX (0, availability);
		}
	}

	h_share->availability = availability;
}

/* ugh, this will be redesigned soon */
void ft_host_share_status (FT_HostShare *h_share, int enabled,
						   int uploads, int max_uploads)
{
	int hs_disabled;
	int hs_uploads;
	int hs_max_uploads;

	if (!h_share)
		return;

	/* figure out where the change was */
	hs_disabled    = h_share->disabled;
	hs_uploads     = h_share->uploads;
	hs_max_uploads = h_share->max_uploads;

	/* ugh...this is getting silly :) */
	if (enabled != -1)
		h_share->disabled = !enabled;
	else if (uploads != -1)
		h_share->uploads = uploads;
	else
		h_share->max_uploads = max_uploads;

	/* detect no change */
	if (hs_disabled == h_share->disabled &&
	    hs_uploads == h_share->uploads &&
	    hs_max_uploads == h_share->max_uploads)
	{
		return;
	}

	/* recalculate availability */
	calc_avail (h_share);
}

/*****************************************************************************/

static void ft_share_free_proto (FileShare *file)
{
	FT_Share *share;

	if (!file)
		return;

	share = share_lookup_data (file, "OpenFT");

	/* it's not safe to free this now...something is still using it */
	if (share->ref)
	{
		TRACE (("unable to free %p, ref count = %i", file, share->ref));
		return;
	}

	free (share->tokens);
	free (share);

	share_remove_data (file, "OpenFT");
}

void ft_share_free (FileShare *file)
{
	ft_share_free_proto (file);

	share_free (file);
}

/*****************************************************************************/

static int share_ref_mod (FileShare *file, int val)
{
	FT_Share *share;

	share = share_lookup_data (file, "OpenFT");

	if (share->ref == 0 && val < 0)
	{
		GIFT_WARN (("file %p requested negative ref count!!!", file));
		return 0;
	}

	share->ref += val;

	return share->ref;
}

void ft_share_ref (FileShare *file)
{
	share_ref_mod (file, 1);
}

void ft_share_unref (FileShare *file)
{
	/* if ref becomes 0, free it */
	if (!share_ref_mod (file, -1))
		ft_share_free (file);
}

/*****************************************************************************/

void ft_share_add (int verified, unsigned long host, unsigned short port,
				   unsigned short http_port, unsigned long size,
				   char *md5, char *filename)
{
	FileShare    *file;
	FT_HostShare *h_share;

	if (!(h_share = ft_host_share_add (verified, host, port, http_port)))
		return;

	/* create the actual fileshare structure for insertion */
	file = ft_share_new (h_share, size, md5, filename);

	if (!ft_share_complete (file))
	{
		ft_share_unref (file);
		return;
	}

	/* insert the fileshare */
	dataset_insert (h_share->dataset, file->sdata->md5, file);
}

#if 0
void ft_share_add_shares (int verified, unsigned long host,
                          unsigned short port, unsigned short http_port,
                          Dataset *shares)
{
	FT_HostShare *h_share;

	if (!shares)
		return;

	if (!(h_share = host_share_add (verified, host, port, http_port)))
		return;

	/* leap of faith here...hope that ft_share_import worked :) */
	h_share->dataset = shares;
}
#endif

/*****************************************************************************/

static int destroy_host_table (unsigned long key, FileShare *file, int *force)
{
	/* if we are shutting down now, force all data out of here */
	if (*force)
	{
		FT_Share *share;

		share = share_lookup_data (file, "OpenFT");
		share->ref = 0;
	}

	/* free if available */
	ft_share_unref (file);

	return TRUE;
}

void ft_share_remove_by_host (unsigned long host, int force)
{
	FT_HostShare *h_share;

	if (!host)
		return;

	/* user is not sharing any files, no big deal */
	if (!(h_share = hash_table_lookup (ft_shares, host)))
		return;

	TRACE (("%s", net_ip_str (host)));

	hash_table_remove (ft_shares, host);

	/* free all the data found in host_table */
	hash_table_foreach_remove (h_share->dataset,
	                           (HashFunc) destroy_host_table, &force);

	ft_host_share_del_db (h_share);
	ft_host_share_free (h_share);
}

/*****************************************************************************/

void ft_share_verified (unsigned long host)
{
	FT_HostShare *h_share;

	if (!host)
		return;

	if (!(h_share = hash_table_lookup (ft_shares, host)))
		return;

	h_share->verified = TRUE;
}

/*****************************************************************************/

/* general purpose function for the routines below...removes the lookup
 * duplication */
static void share_set_status (unsigned long host, int enabled,
                              int uploads, int max_slots)
{
	FT_HostShare *h_share;

	if (!host)
		return;

	if (!(h_share = hash_table_lookup (ft_shares, host)))
	{
		TRACE (("unable to set share status/upload data for %s",
		        net_ip_str (host)));
		return;
	}

	ft_host_share_status (h_share, enabled, uploads, max_slots);
}

void ft_share_disable (unsigned long host)
{
	share_set_status (host, FALSE, -1, -1);
}

void ft_share_enable (unsigned long host)
{
	share_set_status (host, TRUE, -1, -1);
}

void ft_share_set_limit (unsigned long host, int max_slots)
{
	share_set_status (host, -1, -1, max_slots);
}

void ft_share_set_uploads (unsigned long host, int uploads)
{
	share_set_status (host, -1, uploads, -1);
}

/*****************************************************************************/
/* LOCAL SHARES */

void ft_share_local_add (FileShare *file)
{
	FT_HostShare *h_share;

	if (share_lookup_data (file, "OpenFT"))
		return;

	h_share = ft_host_share_new (TRUE, 0, NODE (ft_self)->port,
	                             NODE (ft_self)->http_port);

	ft_share_new_proto (file, h_share);
}

void ft_share_local_remove (FileShare *file)
{
	FT_Share *share;

	if (!(share = share_lookup_data (file, "OpenFT")))
		return;

	/* we cant use ft_share_unref because we're only allowed to remove OpenFT
	 * data...not the actual FileShare */
	if (share->ref > 0)
		share->ref--;

	if (share->ref == 0)
		ft_share_free_proto (file);
}

/*****************************************************************************/

static Connection *local_flush (Connection *c, Node *node, void *udata)
{
	/* remove our shares from this node */
	ft_packet_send (c, FT_MODSHARE_REQUEST, NULL);

	return NULL;
}

void ft_share_local_flush ()
{
	conn_foreach ((ConnForeachFunc) local_flush, NULL,
	              NODE_PARENT, NODE_CONNECTED, 0);
}

/*****************************************************************************/

static Connection *local_sync (Connection *c, Node *node, void *udata)
{
	ft_share_local_submit (c);

	return NULL;
}

void ft_share_local_sync ()
{
	share_comp_write ();

	conn_foreach ((ConnForeachFunc) local_sync, NULL,
	              NODE_PARENT, NODE_CONNECTED, 0);
}

/*****************************************************************************/

static int local_cleanup (unsigned long key, FileShare *file, void *udata)
{
	FT_Share *share;

	if (!(share = share_lookup_data (file, "OpenFT")))
		return TRUE;

	/* we are being unloaded, we _need_ to free this data right now */
	share->ref = 0;

	/* get rid of the extra data */
	ft_share_free_proto (file);

	return TRUE;
}

/* forcefully cleanup all shares */
void ft_share_local_cleanup ()
{
	share_foreach ((HashFunc) local_cleanup, NULL);
}

/*****************************************************************************/

static int local_submit_write (Connection *c, FILE *f, int *comp)
{
	char  *fmt;
	char   buf[RW_BUFFER];
	size_t n;

	/* determine output format */
	fmt = (P_INT (comp) ? "+C" : "+S");

	if (!f)
	{
		int uploads;
		int max_uploads;

		ft_packet_send (c, FT_SHARE_REQUEST, fmt, NULL, 0);

		/* submit upload data here as well */
		uploads     = upload_length (NULL);
		max_uploads = share_status  ();

		ft_packet_send (c, FT_MODSHARE_REQUEST, "%hu%hu%hu",
		                3 /* SUBMIT MAX UPLOADS */, uploads, max_uploads);

		return FALSE;
	}

	if ((n = fread (buf, sizeof (char), sizeof (buf), f)) <= 0)
	{
		fclose (f);
		return FALSE;
	}

	ft_packet_send (c, FT_SHARE_REQUEST, fmt, buf, n);

	/* keep processing... */
	return TRUE;
}

static int local_submit_destroy ()
{
	return FALSE;
}

#if 0
static int local_submit_write (Connection *c, FileShare *file, void *udata)
{
	if (!file)
	{
		ft_packet_send (c, FT_SHARE_REQUEST, NULL);
		return FALSE;
	}

	ft_packet_send (c, FT_SHARE_REQUEST, "%lu%s%s",
	                file->size, file->md5, file->hpath);

	return FALSE;
}

static int local_submit_destroy ()
{
	return FALSE;
}
#endif

static Connection *locate_future_parent (Connection *c, Node *node,
                                         void *udata)
{
	if (node->class & NODE_PARENT)
		return NULL;

	return c;
}

void ft_share_local_submit (Connection *c)
{
	FILE      *f;
	HashTable *shares;
	int        comp_req;
	int        use_comp = FALSE;

	if (!(shares = share_index (NULL, NULL)))
		return;

	if (!c)
	{
		c = conn_foreach ((ConnForeachFunc) locate_future_parent, NULL,
		                  NODE_SEARCH, NODE_CONNECTED, 0);

		/* request this parent authorize us as a child */
		if (c)
			ft_packet_send (c, FT_CHILD_REQUEST, NULL);

		return;
	}

	if (!(NODE (c)->cap))
	{
		TRACE (("warning: potential race condition"));
	}

	/* determine whether or not we should use the compressed shares file */
	comp_req = (dataset_lookup (NODE (c)->cap, "ZLIB") ? TRUE : FALSE);

	/* open the database for transacting */
	if (!(f = fopen (share_comp_path (comp_req, &use_comp), "r")))
	{
		perror ("fopen");
		return;
	}

	TRACE_SOCK (("submitting shares (%s)",
	             (use_comp ? "compressed" : "uncompressed")));

	queue_add_single (c,
	                  (QueueWriteFunc) local_submit_write,
	                  (QueueWriteFunc) local_submit_destroy,
	                  f, I_PTR (use_comp));
}

/*****************************************************************************/
/* TODO - all of this stuff needs to be optimized */

static FT_Stats *stats_new ()
{
	FT_Stats *stats;

	stats = malloc (sizeof (FT_Stats));
	memset (stats, 0, sizeof (FT_Stats));

	return stats;
}

static void stats_free (FT_Stats *stats)
{
	list_free (stats->parents);

	free (stats);
}

/*****************************************************************************/

void ft_share_stats_add (unsigned long parent, unsigned long user,
                         unsigned long shares, unsigned long size)
{
	FT_Stats *stats;

	if (!ft_stats)
		ft_stats = hash_table_new ();

	if (!(stats = hash_table_lookup (ft_stats, user)))
	{
		stats = stats_new ();
		hash_table_insert (ft_stats, user, stats);
	}

	/* hmm, not sure if this really can happen, but just in case */
	if (!list_find (stats->parents, I_PTR (parent)))
		stats->parents = list_append (stats->parents, I_PTR (parent));

	stats->users   = 1;
	stats->shares  = shares;
	stats->size    = (double) size / 1024.0; /* MB -> GB */
}

/*****************************************************************************/

static int stats_remove (unsigned long key, FT_Stats *stats,
                         unsigned long *parent)
{
	/* remove this parent (this may just fail to locate it... either way) */
	stats->parents = list_remove (stats->parents, I_PTR (*parent));

	/* more parents are left, do not get rid of this share data */
	if (stats->parents)
		return FALSE;

	stats_free (stats);
	return TRUE;
}

void ft_share_stats_remove (unsigned long parent, unsigned long user)
{
	FT_Stats *stats;

	/* remove by parent (search node disconnected) */
	if (!user)
	{
		hash_table_foreach_remove (ft_stats, (HashFunc) stats_remove, &parent);
		return;
	}

	if (!(stats = hash_table_lookup (ft_stats, user)))
		return;

	/* only remove it if all search nodes agree that it should be removed */
	if (stats_remove (0, stats, &parent))
		hash_table_remove (ft_stats, user);
}

/*****************************************************************************/

static int stats_get (unsigned long key, FT_Stats *stats,
                      FT_Stats *ret)
{
	if (!stats)
		return TRUE;

	ret->users  += stats->users;
	ret->shares += stats->shares;
	ret->size   += stats->size;

	return TRUE;
}

/* TODO - opt...this is written in such a way that ensures correctness, but
 * is much slower than a global counter of some kind */
void ft_share_stats_get (unsigned long *user, unsigned long *shares,
                         double *size)
{
	FT_Stats stats;

	memset (&stats, 0, sizeof (stats));

	hash_table_foreach (ft_stats, (HashFunc) stats_get, &stats);

	if (user)
		*user = stats.users;
	if (shares)
		*shares = stats.shares;
	if (size)
		*size = stats.size;
}

/*****************************************************************************/

static int stats_get_digest (unsigned long key, FileShare *file,
                             FT_Stats *stats)
{
	stats->shares++;
	stats->size += ((float)file->size / 1024.0) / 1024.0;

	return TRUE;
}

/* TODO -- have this pre-calculated by each addition... doing it this way is
 * damn lame */
void ft_share_stats_get_digest (unsigned long host, unsigned long *shares,
                                double *size, int *uploads, int *max_uploads)
{
	FT_HostShare *h_share;
	FT_Stats      stats;

	if (!host)
		return;

	if (!(h_share = hash_table_lookup (ft_shares, host)))
		return;

	memset (&stats, 0, sizeof (stats));

	hash_table_foreach (h_share->dataset,
	                    (HashFunc) stats_get_digest, &stats);

	if (shares)
		*shares = stats.shares;
	if (size)
		*size = stats.size; /* MB */
	if (uploads)
		*uploads = h_share->uploads;
	if (max_uploads)
		*max_uploads = h_share->max_uploads;
}
