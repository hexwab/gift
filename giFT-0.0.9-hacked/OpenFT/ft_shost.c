/*
 * $Id: ft_shost.c,v 1.31 2003/05/30 02:26:51 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#include "ft_openft.h"

#include "lib/stopwatch.h"

#include "ft_search.h"
#include "ft_search_exec.h"

#include "ft_shost.h"
#include "ft_search_db.h"

#ifdef BENCHMARK
#undef FT_MAX_CHILDREN
#define FT_MAX_CHILDREN 10000
#endif

/*****************************************************************************/

/*
 * We hold the dataset of all child users and a reference to their primary
 * database in memory so that we can more easily manipulate the extremely
 * fluctuating user base.
 *
 * NOTE:
 * This is no longer utilized for browsing, as that has been moved to
 * "direct" browsing where the user responding is the user with the shares.
 *
 * NOTE2:
 * This whole file may be able to go away soon, as it's really not
 * required now that we've broken apart the share databases into the
 * master/primary split.
 */
static Dataset *shosts = NULL;

static FTSHost **shosts_array = NULL;
static int shosts_count = 0;
static int shosts_size = 0;
static int last_index = 0; /* attempt to speed up insertion slightly */

/*****************************************************************************/

Dataset *ft_shosts ()
{
	return shosts;
}

/*****************************************************************************/

FTSHost *ft_shost_new (int verified, in_addr_t host,
					   in_port_t ft_port, in_port_t http_port,
					   char *alias)
{
	FTSHost *shost;

	if (!(shost = MALLOC (sizeof (FTSHost))))
		return NULL;

	shost->verified  = verified;
	shost->host      = host;
	shost->ft_port   = ft_port;
	shost->http_port = http_port;
	shost->alias     = STRDUP (alias);

	return shost;
}

#if 0
static int remove_active_files (FileShare *file, FTSHost *shost)
{
	FTShare *share;

	/* nullify the shost object -- this result is no longer valid but for
	 * design purposes we can't unload it from the search queue */
	if ((share = share_lookup_data (file, "OpenFT")))
		share->shost = NULL;

	return TRUE;
}
#endif

void ft_shost_free (FTSHost *shost)
{
#ifdef USE_LIBDB
	ft_search_db_close (shost, TRUE);
#endif /* USE_LIBDB */

#if 0
	list_foreach_remove (shost->files, (ListForeachFunc)remove_active_files,
	                     shost);
#endif

	free (shost->alias);
	free (shost);
}

/*****************************************************************************/

static FTSHost *lookup (in_addr_t ip)
{
	FTSHost *shost;

	if (!ip)
		return NULL;

	if (!(shost = dataset_lookup (shosts, &ip, sizeof (ip))))
		return NULL;

	return shost;
}

static void insert (in_addr_t ip, FTSHost *shost)
{
	if (!ip || !shost)
		return;

	if (!shosts)
		shosts = dataset_new (DATASET_HASH);

	dataset_insert (&shosts, &ip, sizeof (ip), shost, 0);
}

static void del (in_addr_t ip)
{
	if (!ip)
		return;

	dataset_remove (shosts, &ip, sizeof (ip));
}

static int insert_array (FTSHost *shost)
{
	if (!shosts_array || shosts_count == shosts_size)
	{
		int old_size = shosts_size;
		if (shosts_array)
			shosts_size <<= 1;
		else
			shosts_size = FT_MAX_CHILDREN;
		
		shosts_array = REALLOC (shosts_array, shosts_size * sizeof(FTSHost*));
		if (shosts_array)
		{
			int i;
			for (i=old_size; i<shosts_size; i++)
				shosts_array[i] = NULL;
		}
	}
	
	if (shosts_array)
	{
		int i;
		
		for (i = last_index ; ; i++)
		{
			if (i == shosts_size)
				i = 0;
			
			if (!shosts_array[i])
				break;
		}
		
		shosts_array[i] = shost;

		shosts_count++;
		last_index = (i + 1) % shosts_size;

		return i;
	} else
		return -1;
}

static void del_array (int index)
{
	assert (index >=0 && index < shosts_size);

	shosts_array[index] = NULL;
	shosts_count--;
}


/*****************************************************************************/

FTSHost *ft_shost_get (in_addr_t ip)
{
	return lookup (ip);
}

FTSHost *ft_shost_get_index (int index)
{
	assert (index < shosts_size);
	return shosts_array[index];
}

int ft_shost_add (FTSHost *shost)
{
	if (!shost)
		return FALSE;

	if (lookup (shost->host))
		return FALSE;

	insert (shost->host, shost);

	shost->index = insert_array (shost);

	FT->DBGFN (FT, "new shost index for %s is %d", net_ip_str(shost->host), shost->index);

	return TRUE;
}

static void remove_complete (FTSHost *shost)
{
	assert (shost != NULL);

	del (shost->host);
	del_array (shost->index);

	ft_shost_free (shost);

	/* sync the master databases for debugging */
	ft_search_db_sync (NULL);
}

void ft_shost_remove (in_addr_t ip)
{
	FTSHost *shost;

	if (!(shost = lookup (ip)))
		return;

	if (shost->dirty)
	{
		FT->DBGFN (FT, "attempted to remove %s (dirty)", net_ip_str (ip));
		return;
	}

	/* mark this shost for removal, refusing to return search results
	 * from it */
	shost->dirty = TRUE;

	/* remove_complete will be called automagically once the share host has
	 * been completely cleaned out of the databases */
	if (!(ft_search_db_remove_host (shost, remove_complete)))
	{
		FT->DBGFN (FT, "%s: unable to remove shost db entries",
		           net_ip_str (ip));
		remove_complete (shost);
	}

}

/*****************************************************************************/

void ft_shost_verified (in_addr_t ip, in_port_t ft_port,
                        in_port_t http_port, int verified)
{
	FTSHost *shost;

	if (!(shost = lookup (ip)))
		return;

	shost->ft_port   = ft_port;
	shost->http_port = http_port;
	shost->verified  = verified;
}

void ft_shost_avail (in_addr_t ip, unsigned long avail)
{
	FTSHost *shost;

	if (!(shost = lookup (ip)))
		return;

	shost->availability = avail;
}

int ft_shost_digest (in_addr_t ip, unsigned long *shares,
                     double *size, unsigned long *avail)
{
	FTSHost *shost;

	if (!(shost = lookup (ip)))
		return FALSE;

	if (shares)
		*shares = shost->shares;

	if (size)
		*size = shost->size;

	if (avail)
		*avail = shost->availability;

	return TRUE;
}
