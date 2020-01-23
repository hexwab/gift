/*
 * $Id: as_push_man.c,v 1.1 2004/09/26 19:49:37 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* Allocate and init push manager. */
ASPushMan *as_pushman_create ()
{
	ASPushMan *man;

	if (!(man = malloc (sizeof (ASPushMan))))
		return NULL;

	if (!(man->pushes = as_hashtable_create_int ()))
	{
		free (man);
		return NULL;
	}

	man->next_push_id = INVALID_PUSH_ID + 1;

	return man;
}

static as_bool push_free_itr (ASHashTableEntry *entry, void *udata)
{
	as_push_free ((ASPush *) entry->val);
	return TRUE; /* remove entry */
}

/* Free manager. */
void as_pushman_free (ASPushMan *man)
{
	if (!man)
		return;

	/* free all remaining pushes */
	as_hashtable_foreach (man->pushes, push_free_itr, NULL);
	as_hashtable_free (man->pushes, FALSE);
	free (man);
}

/*****************************************************************************/

/* Send push request for hash to source. push_cb is called with the result. */
ASPush *as_pushman_send (ASPushMan *man, ASPushCb push_cb, ASSource *source,
                         ASHash *hash)
{
	ASPush *push;

	/* Create push. */
	if (!(push = as_push_create (man->next_push_id, source, hash, push_cb)))
		return NULL;

	/* Insert into hash table. */
	if (!as_hashtable_insert_int (man->pushes, push->id, push))
	{
		AS_ERR ("Hashtable insert failed for new push");
		as_push_free (push);

		assert (0);
		return NULL;
	}

	/* Send push. */
	if (!as_push_send (push))
	{
		as_pushman_remove (man, push);
		return NULL;
	}

	/* We actually used this id, go to next. */
	man->next_push_id++;

	return push;
}

/* Remove and free push. */
void as_pushman_remove (ASPushMan *man, ASPush *push)
{
	ASPush *hp;

	if (!push)
		return;

	/* Remove push from hash table. */
	if (!(hp = as_hashtable_remove_int (man->pushes, push->id)))
	{
		AS_WARN_1 ("Couldn't remove push with id %d from hash table",
		           (int)push->id);
		/* this is a bug somewhere else */
		assert (0);
	}

	assert (hp == push);

	/* Free push. */
	as_push_free (push);
}

/*****************************************************************************/

/* Handle pushed concection. */
as_bool as_pushman_accept (ASPushMan *man, ASHash *hash, as_uint32 push_id,
                           TCPC *c)
{
	ASPush *push;

	/* Find push. */
	if (!(push = as_pushman_lookup (man, push_id)))
	{
		AS_HEAVY_DBG_1 ("Couldn't find incoming push %d", push_id);
		return FALSE;
	}

	/* Hand over connection to push and its callback */
	return as_push_accept (push, hash, c); /* may free and remove push */
}

/*****************************************************************************/

/* Get push by its id. */
ASPush *as_pushman_lookup (ASPushMan *man, as_uint32 push_id)
{
	ASPush *push;

	if (push_id == INVALID_PUSH_ID)
		return NULL;

	if (!(push = as_hashtable_lookup_int (man->pushes, push_id)))
		return NULL;

	return push;
}

/*****************************************************************************/
