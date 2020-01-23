/*
 * $Id: as_share_man.c,v 1.16 2005/11/05 20:12:50 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/***********************************************************************/

ASShareMan *as_shareman_create (void)
{
	ASShareMan *man = malloc (sizeof (ASShareMan));

	if (!man)
		return NULL;

	man->shares  = NULL;
	man->table   = as_hashtable_create_mem (FALSE);
	man->nshares = 0;
	man->size    = 0;

	return man;
}

static int free_share (ASShare *share, void *udata)
{
	as_share_free (share);
	return TRUE;
}

void as_shareman_free (ASShareMan *man)
{
	as_hashtable_free (man->table, FALSE);
	list_foreach_remove (man->shares, (ListForeachFunc)free_share, NULL);
	free (man);
}

/***********************************************************************/

/* Add share to manager. Fails if a share with the same hash already
 * exists otherwise takes ownership of share.
 */
as_bool as_shareman_add (ASShareMan *man, ASShare *share)
{
	List *link;

	if ((link = as_hashtable_lookup (man->table, share->hash->data,
	                                 sizeof (share->hash->data))))
	{
		AS_HEAVY_DBG_1 ("Duplicate hash share '%s'", share->path);
		return FALSE;
	}

	/* Create new list entry and add it to hash table. */
	man->shares = list_prepend (man->shares, share);

	if (!as_hashtable_insert (man->table, share->hash->data,
	                          sizeof (share->hash->data),
	                          man->shares))
	{
		AS_ERR_1 ("Hashtable insert failed for share '%s'", share->path);
		assert (0);
		return FALSE;
	}

	man->nshares++;

	man->size += ((double)share->size) / 1048576;

	return TRUE;
}

/* Remove and free share with specified hash. */
as_bool as_shareman_remove (ASShareMan *man, ASHash *hash)
{
	List *link;
	ASShare *share;

	/* Lookup and remove list link from hashtable. */
	if (!(link = as_hashtable_remove (man->table, hash->data,
	                                  sizeof (hash->data))))
	{
		AS_ERR_1 ("Didn't find share '%s' for removal.",
		          as_hash_str (hash));
		return FALSE;
	}

	share = link->data;

	/* Free share */
	man->nshares--;
	man->size -= ((double)share->size) / 1048576;
	as_share_free (share);

	/* remove link */
	man->shares = list_remove_link (man->shares, link);

	return TRUE;
}

/* Remove and free all shares. */
void as_shareman_remove_all (ASShareMan *man)
{
	as_hashtable_free (man->table, FALSE);
	man->table = as_hashtable_create_mem (FALSE);

	list_foreach_remove (man->shares, (ListForeachFunc)free_share, NULL);
	man->shares = NULL;
}

/* Lookup share by file hash. */
ASShare *as_shareman_lookup (ASShareMan *man, ASHash *hash)
{
	List *link;

	if (!(link  = as_hashtable_lookup (man->table, hash->data,
	                                   sizeof (hash->data))))
	{
		return NULL;
	}

	return link->data;
}

/***********************************************************************/

typedef struct
{
	ASSession *session;
	ASPacket  *data;
} Conglobulator;

/* The maximum glob size after which we compress and send; note that
 * this is only vaguely related to maximum packet size.
 */
#define GLOB_MAX 4096

static as_bool conglobulator_flush (Conglobulator *glob)
{
	int ret = TRUE;

	if (glob->data)
	{
		ret = as_session_send (glob->session, PACKET_COMPRESSED,
		                       glob->data, PACKET_COMPRESS);

		as_packet_free (glob->data);
		glob->data = NULL;
	}

	return ret;
}

static as_bool conglobulator_assimilate (Conglobulator *glob, ASPacket *p)
{
	if (!glob->data)
		glob->data = p;
	else
	{
		as_packet_append (glob->data, p);
		as_packet_free (p);
	}

	if (glob->data->used > GLOB_MAX)
		return conglobulator_flush (glob);

	return TRUE;
}

static int share_send (ASShare *share, Conglobulator *glob)
{
	ASPacket *p;

	if (!share)
		return FALSE;

	if (!(p = as_share_packet (share)))
		return FALSE;

	as_packet_header (p, PACKET_SHARE);
	
	return conglobulator_assimilate (glob, p);
}

static as_bool submit_share_list (ASSession *session, List *shares)
{
	Conglobulator glob = { session, NULL };

	/* dammit, WTF does this return void?! */
	list_foreach (shares, (ListForeachFunc)share_send, &glob);

	conglobulator_flush (&glob);

	return TRUE;
}

/* Submit all shares to specified supernode. */
as_bool as_shareman_submit (ASShareMan *man, ASSession *session)
{
	if (submit_share_list (session, man->shares))
	{
		AS_DBG_2 ("Submitted all %d shares to supernode %s",
		          man->nshares, net_ip_str (session->host));
		return TRUE;
	}

	return FALSE;
}

/* Submit list of shares to all connected supernodes and add shares to
 * manager. Takes ownership of list values (shares).  Shares in the
 * list that failed to be added (e.g. because they had duplicate hashes) will
 * be freed and replaced by a NULL pointer.
 */
as_bool as_shareman_add_and_submit (ASShareMan *man, List *shares)
{
	int sessions, ok = 0, total = 0;
	List *link;

	/* Add shares to manager. */
	for (link = shares; link; link = link->next)
	{
		if (as_shareman_add (man, link->data))
			ok++;
		else
		{
			as_share_free (link->data);
			link->data = NULL;
		}

		total++;
	}

	/* Sent shares to all connected sesssions. */
	sessions = as_sessman_foreach (AS->sessman, 
	                               (ASSessionForeachFunc)submit_share_list,
	                               shares);

	AS_DBG_3 ("Submitted %d of %d shares to %d supernodes.", ok, total,
	          sessions);
		
	return TRUE;
}

/***********************************************************************/

