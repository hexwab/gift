/*
 * $Id: ft_netorg.c,v 1.39 2004/08/02 23:59:27 hexwab Exp $
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

#include "ft_netorg.h"

/*****************************************************************************/

/*
 * We are dividing each list into it's own list structure and assigning a
 * pointer to the last processed element so that we can traverse quickly (and
 * over multiple times) through all connections of a particular connection
 * state.  Otherwise, we would be required to iterate through thousands of
 * potentially useless nodes looking for what we want.
 */
struct conn_list
{
	ListLock *list;                    /* actual list of nodes */
	List     *iptr;                    /* pointer to last processed chain */
	int       count;                   /* number of entries in list */
};

static struct conn_list final;         /* FT_NODE_CONNECTED */
static struct conn_list limbo;         /* FT_NODE_CONNECTING */
static struct conn_list disco;         /* FT_NODE_DISCONNECTED */

/*
 * Additionally, we will want to hold all nodes in a table designed for fast
 * lookups, and for that we are using a Dataset (backend DATASET_HASH).
 * ::ft_netorg_lookup cannot be expensive.
 */
static Dataset *nodes = NULL;

/*****************************************************************************/

/*
 * Hardcode the necessary size requirements for nodes_len.  If FTNodeState or
 * ft_class_t values change, this must be updated.
 */
#define STATE_MAX            (4)       /* maximum value for FTNodeState */
#define KLASS_MAX            (31)      /* maximum value for ft_class_t after
                                        * the necessary shifting has occurred */

/*
 * Maintain an additional data structure which tracks the total size of any
 * possible list that can be constructed with the API present in
 * ft_netorg_foreach or ft_netorg_length.  This is maintained so that
 * ft_netorg_length does not require iteration over each node in the
 * state set.
 *
 * The second index in this array does not directly map to the class.
 * Instead, a shifting occurs to reduce the maximum size for the upper 3
 * bits.  See the macro KLASS_INDEX for more information on how this
 * translation occurs.
 */
static unsigned int nodes_len[STATE_MAX+1][KLASS_MAX+1] = { { 0, 0 } };

/*
 * Apply the necessary shifting of the enumerated sets so that there are no
 * unused bits.  If the way FTNodeState or ft_class_t is defined changes, so
 * must this macro.
 */
#define STATE_INDEX(state)   (int)(state)
#define KLASS_INDEX(klass)   \
	(int)((((klass) & 0x700) >> 6) | (((klass) & 0x6) >> 1))

/*
 * If enabled the consistency of the nodes_len cache is verified against
 * ft_netorg_foreach on every call to ft_netorg_length.  This obviously
 * removes all performance benefits of having a cache.
 *
 * FIXME: Enabling this changes the behaviour of ft_netorg_foreach so it
 *        can be used to verify the cache.  This _at least_ prevents stats
 *        from being collected correctly.  Other side effects are possible!
 */
/* #define VERIFY_NETORG_LENGTH */

/*****************************************************************************/

static BOOL netorg_valid (FTNode *node)
{
	if (!node || node->ninfo.host == 0)
		return FALSE;

	return TRUE;
}

/*****************************************************************************/

static struct conn_list *get_conn_list (ft_state_t state)
{
	struct conn_list *list_addr;

	/* set the list_addr value so that we may either return the initialized
	 * list or construct one ourselves */
	switch (state)
	{
	 case FT_NODE_CONNECTED:    list_addr = &final; break;
	 case FT_NODE_CONNECTING:   list_addr = &limbo; break;
	 case FT_NODE_DISCONNECTED: list_addr = &disco; break;
	 default:                   list_addr = NULL;   break;
	}

	assert (list_addr != NULL);

	/* apready non-NULL, no need to do anything but return it */
	if (list_addr->list)
		return list_addr;

	/* construct the list lock */
	if (!(list_addr->list = list_lock_new ()))
		return NULL;

	list_addr->iptr = NULL;
	return list_addr;
}

/*****************************************************************************/

static int add_sorted (FTNode *a, FTNode *b)
{
	int ret;

	/* sort by version, last_session, then uptime */
	if ((ret = FT_VERSION_CMP(a->version, b->version)))
		return -ret;

	if ((ret = INTCMP(a->last_session, b->last_session)))
		return -ret;

	if ((ret = INTCMP(a->uptime, b->uptime)))
		return -ret;

	/* equal */
	return 0;
}

static int add_conn_list (ft_state_t state, FTNode *node)
{
	struct conn_list *clist;

	/* attempt to retrieve the appropriate conn_list pointer for the inputted
	 * state */
	if (!(clist = get_conn_list (state)))
		return FALSE;

	/* list_lock currently has some pretty nasty bugs with this function,
	 * eventually the will need to be fixed */
	list_lock_insert_sorted (clist->list, (CompareFunc)add_sorted, node);
	clist->count++;

	/* initialize the list pointer to the very first chain  if necessary */
	if (!clist->iptr)
		clist->iptr = clist->list->list;

	return TRUE;
}

static void update_nodes_len (FTNode *node,
                              ft_class_t klass,      ft_state_t state,
                              ft_class_t klass_orig, ft_state_t state_orig)
{
	int idx_klass      = KLASS_INDEX(klass);
	int idx_klass_orig = KLASS_INDEX(klass_orig);
	int idx_state      = STATE_INDEX(state);
	int idx_state_orig = STATE_INDEX(state_orig);

	/*
	 * Because we squash FT_NODE_USER, we must make sure that the original
	 * class is set before we apply the index value (which may be 0 for a
	 * non-zero class input).  We require that the new class at least contain
	 * FT_NODE_USER.
	 */
	if (klass_orig)
	{
		/*
		 * NOTE: FT_NODE_STATEANY (0) is a special case which represents all
		 * states.  It feels wrong to rely on the identifier FT_NODE_STATEANY
		 * to be defined as 0, so we will hardcode its current value here.
		 */
		nodes_len[idx_state_orig][idx_klass_orig]--;
		nodes_len[0             ][idx_klass_orig]--;
	}

	assert (klass > 0);
	nodes_len[idx_state][idx_klass]++;
	nodes_len[0        ][idx_klass]++;
}

void ft_netorg_add (FTNode *node)
{
	if (!(netorg_valid (node)))
		return;

	/* add to the appropriate state-specific conn list */
	if (!add_conn_list (node->state, node))
		return;

	update_nodes_len (node, node->ninfo.klass, node->state, 0, node->state);

	/* add to the lookup hash table */
	if (!nodes)
		nodes = dataset_new (DATASET_HASH);

	dataset_insert (&nodes, &node->ninfo.host, sizeof (in_addr_t), node, 0);
}

/*****************************************************************************/

static void rem_conn_list (ft_state_t state, FTNode *node)
{
	struct conn_list *clist;

	if (!(clist = get_conn_list (state)))
		return;

	/*
	 * Before we can remove this node we need to check if the current
	 * iterator is resting on the node we are trying to delete.  If that is
	 * the case, we will reset it's value to NULL and then handle that
	 * special condition after removal (just in case the first node is also
	 * the one we are removing).
	 */
	if (clist->iptr && clist->iptr->data == node)
		clist->iptr = NULL;

	list_lock_remove (clist->list, node);
	clist->count--;

	if (!clist->iptr)
		clist->iptr = clist->list->list;
}

void ft_netorg_remove (FTNode *node)
{
	if (!(netorg_valid (node)))
		return;

	rem_conn_list (node->state, node);

	/*
	 * CHECKME: This should immediately fail the assert (klass > 0) in
	 *          update_nodes_len.  However it seems we currently never remove
	 *          nodes so this is not called at all.  Still removing nodes
	 *          should not add them to another field of nodes_len.
	 */
	update_nodes_len (node, 0, node->state, node->ninfo.klass, node->state);

	dataset_remove (nodes, &node->ninfo.host, sizeof (in_addr_t));
}

/*****************************************************************************/

void ft_netorg_change (FTNode *node,
                       ft_class_t class_orig,
                       ft_state_t state_orig)
{
	if (!(netorg_valid (node)))
		return;

	update_nodes_len (node,
	                  node->ninfo.klass, node->state,
	                  class_orig,        state_orig);

	if (node->state != state_orig)
	{
		rem_conn_list (state_orig, node);
		add_conn_list (node->state, node);
	}
}

/*****************************************************************************/

FTNode *ft_netorg_lookup (in_addr_t ip)
{
	if (ip == 0)
		return NULL;

	return dataset_lookup (nodes, &ip, sizeof (ip));
}

/*****************************************************************************/

static int conn_list_iter (struct conn_list *clist,
                           ft_class_t klass, ft_state_t state, int iter,
                           FTNetorgForeach func, void *udata)
{
	List *start, *ptr;
	int   looped = FALSE;
	int   ret;
	int   processed = 0;

	/* if iter is non-zero, we were asked to iterate a set number of nodes
	 * and we should therefore begin from the place we last left off.
	 * otherwise, we start from the very beginning of the current list */
	if (iter)
		start = clist->iptr;
	else
		start = (clist->list ? clist->list->list : NULL);

	/* loop through all nodes in the clist provided, beginning at start but
	 * optionally moving to the beginning of the list again, if warranted */
	for (ptr = start ;; ptr = list_next (ptr))
	{
		FTNode *node;

		/* if iter was supplied, we need the ability to begin looping
		 * from the start of the list again, if we reach the end */
		if (iter && !ptr && !looped)
		{
			ptr = (clist->list ? clist->list->list : NULL);
			looped = TRUE;
		}

		/* end of list */
		if (!ptr)
			break;

		/* we wrapped to the beginning, but have just reached the original
		 * start so we should bail out */
		if (looped && ptr == start)
			break;

		node = ptr->data;
		assert (node != NULL);

		/*
		 * Filter by klass, if non-zero.
		 *
		 * WARNING: If VERIFY_NETORG_LENGTH is defined we filter klass
		 *          differently.  This has side effects on the rest of OpenFT.
		 *          See the #define at the top of this file for details.
		 */
#ifdef VERIFY_NETORG_LENGTH
		if (klass && (node->ninfo.klass & klass) != klass)
#else
		if (klass && (node->ninfo.klass & klass) == 0)
#endif
			continue;

		/*
		 * While we were iterating this list (it's locked, remember) a state
		 * changed must have occurred for this node that will not be
		 * reflected in this iteration.  Ideally, we could check to make sure
		 * this node is in the lock removal list, but for now I think it's
		 * safe to just ignore the condition.
		 */
		if (node->state != state)
			continue;

		/*
		 * Execute the callback requested for each node satisfying the above
		 * conditions.  The return value of this is indicates whether or not
		 * the above filtering was "enough" to actually process the node.  If
		 * we return FALSE here, iter should be left alone.
		 */
		if (!(ret = func (node, udata)))
			continue;

		/* increment the total number of nodes processed by this function,
		 * and bail out if we have reached our desired limit */
		processed++;
		if (iter)
		{
			if (processed >= iter)
				break;

			/* move the iterator along with the local ptr, but only after a
			 * node has been processed (see above) */
			clist->iptr = ptr->next;
		}
	}

	return processed;
}

static int foreach_list (ft_class_t klass, ft_state_t state, int iter,
                         FTNetorgForeach func, void *udata)
{
	struct conn_list *clist = NULL;
	int ret;

	if (!(clist = get_conn_list (state)))
		return 0;

	/* lock write access to the soon to be iterating list so that `func' is
	 * unable to manipulate it at this very moment, as many times it will
	 * want to change classes, remove nodes, etc */
	list_lock (clist->list);

	/* loop through all nodes that match klass and state up to iter (if
	 * originally non-zero), calling the user supplied func while passing
	 * along udata */
	ret = conn_list_iter (clist, klass, state, iter, func, udata);

	/* check to make sure if the iterator was scheduled for removal */
	if (clist->iptr)
	{
		assert (clist->list != NULL);

		if (list_find (clist->list->lock_remove, clist->iptr->data))
			clist->iptr = NULL;
	}

	/* allow further writes to be performed, as well as merging all queued
	 * writes into the structure */
	list_unlock (clist->list);

	/* move the iterator back to the beginning of the list (if it was removed
	 * from the condition above) */
	if (!clist->iptr)
		clist->iptr = (clist->list ? clist->list->list : NULL);

	return ret;
}

static int iter_state (ft_class_t klass, ft_state_t state, int iter, int *looped,
                       FTNetorgForeach func, void *udata)
{
	int ret;
	int allow = 0;

	if (iter > 0)
	{
		if ((allow = iter - *looped) <= 0)
			return 0;
	}

	ret = foreach_list (klass, state, allow, func, udata);
	*looped += ret;

	return ret;
}

int ft_netorg_foreach (ft_class_t klass, ft_state_t state, int iter,
                       FTNetorgForeach func, void *udata)
{
	int looped = 0;

	if (!func)
		return 0;

	if (state)
		looped = foreach_list (klass, state, iter, func, udata);
	else
	{
		/* state = 0x00 indicates that all states should be processed,
		 * therefore requiring multiple calls to foreach_list that are
		 * capable of persisting iteration data */
		iter_state (klass, FT_NODE_CONNECTED, iter, &looped, func, udata);
		iter_state (klass, FT_NODE_CONNECTING, iter, &looped, func, udata);
		iter_state (klass, FT_NODE_DISCONNECTED, iter, &looped, func, udata);
	}

	if (iter)
		assert (looped <= iter);

	return looped;
}

/*****************************************************************************/

#ifdef VERIFY_NETORG_LENGTH
static BOOL length_dummy (FTNode *node, void *udata)
{
	return TRUE;
}
#endif /* VERIFY_NETORG_LENGTH */

static void randomize_conn_iptr (ft_state_t state)
{
	struct conn_list *clist = NULL;
	int new_pos;

	if (!(clist = get_conn_list (state)))
		return;

	if (clist->list == NULL || clist->count == 0)
		return;

	/* set clist->iptr to random point in list */
	new_pos = rand() % clist->count;
	clist->iptr = list_nth (clist->list->list, new_pos);

	/* just in case anything went wrong */
	if (clist->iptr == NULL)
		clist->iptr = (clist->list ? clist->list->list : NULL);
}

int ft_netorg_random (ft_class_t klass, ft_state_t state, int iter,
                      FTNetorgForeach func, void *udata)
{
	int looped = 0;

	if (!func)
		return 0;

	if (state)
	{
		randomize_conn_iptr (state);
		looped = foreach_list (klass, state, iter, func, udata);
	}
	else
	{
		/* state = 0x00 indicates that all states should be processed,
		 * therefore requiring multiple calls to foreach_list that are
		 * capable of persisting iteration data */
		randomize_conn_iptr (FT_NODE_CONNECTED);
		randomize_conn_iptr (FT_NODE_CONNECTING);
		randomize_conn_iptr (FT_NODE_DISCONNECTED);

		iter_state (klass, FT_NODE_CONNECTED, iter, &looped, func, udata);
		iter_state (klass, FT_NODE_CONNECTING, iter, &looped, func, udata);
		iter_state (klass, FT_NODE_DISCONNECTED, iter, &looped, func, udata);
	}

	if (iter)
		assert (looped <= iter);

	return looped;
}

/*****************************************************************************/

int ft_netorg_length (ft_class_t klass, ft_state_t state)
{
	int klass_idx, state_idx;
	int klass_test;
	int len = 0;
#ifdef VERIFY_NETORG_LENGTH
	int lenvfy;
#endif

	state_idx = STATE_INDEX(state);
	assert (state_idx >= 0);
	assert (state_idx <= STATE_MAX);

#ifdef VERIFY_NETORG_LENGTH
	lenvfy = ft_netorg_foreach (klass, state, 0,
	                            FT_NETORG_FOREACH(length_dummy), NULL);
#endif /* VERIFY_NETORG_LENGTH */

	/*
	 * Because of the way we squash FT_NODE_USER, we have to match
	 * FT_NODE_CLASSANY (0x0) as a special case and setup the index so that
	 * it will match for all possible klass combinations.
	 */
	if (klass == FT_NODE_CLASSANY)
		klass_idx = 0;
	else
		klass_idx = KLASS_INDEX(klass);

	assert (klass_idx >= 0);
	assert (klass_idx <= KLASS_MAX);

	/*
	 * Loop through every possible klass combination testing for which
	 * indices satisfy the input klass requrement.  The way the nodes_len
	 * data structure is built has each possible klass combination maintained
	 * independently so that FT_NODE_USER and FT_NODE_USER | FT_NODE_SEARCH,
	 * for example, would be mutually exclusive.  This means that we have to
	 * test for every combination that has FT_NODE_USER as a flag, and add
	 * its value to the running total.
	 */
	for (klass_test = 0; klass_test <= KLASS_MAX; klass_test++)
	{
		if ((klass_idx & klass_test) != klass_idx)
			continue;

		len += nodes_len[state_idx][klass_test];
	}

#ifdef VERIFY_NETORG_LENGTH
	assert (len == lenvfy);
#endif

	return len;
}

/*****************************************************************************/

static void free_conn_list (struct conn_list *clist)
{
	list_lock_free (clist->list);
	memset (clist, 0, sizeof (struct conn_list));
}

void ft_netorg_clear (FTNetorgForeach func, void *udata)
{
	ft_netorg_foreach (FT_NODE_CLASSANY, FT_NODE_STATEANY, 0, func, udata);

	free_conn_list (&final);
	free_conn_list (&limbo);
	free_conn_list (&disco);

	dataset_clear (nodes);
	nodes = NULL;
}

/*****************************************************************************/

#ifdef OPENFT_DEBUG
static int dump_node (FTNode *node, void *udata)
{
	FT->DBGFN (FT, "%s: %hu", ft_node_fmt (node), node->ninfo.klass);
	return TRUE;
}

void ft_netorg_dump (void)
{
	ft_netorg_foreach (FT_NODE_CLASSANY, FT_NODE_STATEANY, 0,
	                   FT_NETORG_FOREACH(dump_node), NULL);
}
#endif /* OPENFT_DEBUG */
