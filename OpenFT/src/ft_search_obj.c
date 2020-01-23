/*
 * $Id: ft_search_obj.c,v 1.20 2004/08/04 01:56:25 hexwab Exp $
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

#include "ft_guid.h"
#include "ft_search_exec.h"

#include "ft_search_obj.h"
#include "ft_tokenize.h"

/*****************************************************************************/

static Dataset *searches = NULL;       /* direct local searches */
static Dataset *browses  = NULL;       /* direct local browses */
static Dataset *forwards = NULL;       /* forwarded search queries */

static timer_id fwd_timeout_timer = 0; /* expire search forward queries */

/*****************************************************************************/

static void set_params (ft_search_parms_t *dst, ft_search_flags_t type,
                        const char *realm,
                        const char *query, const char *exclude)
{
	assert (type != 0x00);
	assert (query != NULL);

	dst->type    = type;
	dst->realm   = STRDUP (realm);
	dst->query   = STRDUP (query);
	dst->exclude = STRDUP (exclude);
	dst->qtokens = ft_tokenize_query (query, 0);
	dst->etokens = ft_tokenize_query (exclude, 0);
}

static int search_timeout (FTSearch *srch)
{
	ft_search_finish (srch);
	return FALSE;
}

static int search_begin (FTSearch *srch)
{
	assert (dataset_lookup (searches, srch->guid, FT_GUID_SIZE) == NULL);
	dataset_insert (&searches, srch->guid, FT_GUID_SIZE, srch, 0);

	srch->timeout =
	    timer_add (3 * MINUTES, (TimerCallback)search_timeout, srch);

	return TRUE;
}

FTSearch *ft_search_new (IFEvent *event, ft_search_flags_t type,
                         const char *realm,
                         const char *query, const char *exclude)
{
	FTSearch *srch;

	if (!(srch = MALLOC (sizeof (FTSearch))))
		return NULL;

	srch->event = event;               /* gift reply handle */
	srch->guid  = ft_guid_new ();      /* everything-else handle */
	set_params (&srch->params, type, realm, query, exclude);

	search_begin (srch);

	return srch;
}

static void finish_params (ft_search_parms_t *params)
{
	free (params->realm);
	free (params->query);
	free (params->exclude);
	ft_tokenize_free (params->qtokens);
	ft_tokenize_free (params->etokens);
}

static void search_free (FTSearch *srch)
{
	ft_guid_free (srch->guid);
	finish_params (&srch->params);
	dataset_clear (srch->waiting_on);

	free (srch);
}

void ft_search_finish (FTSearch *srch)
{
	if (!srch)
		return;

#if 0
	FT->DBGFN (FT, "%s", ft_guid_fmt (srch->guid));
#endif

	timer_remove_zero (&srch->timeout);

	if (srch->event)
		FT->search_complete (FT, srch->event);

	dataset_remove (searches, srch->guid, FT_GUID_SIZE);

	search_free (srch);
}

/*****************************************************************************/

FTSearch *ft_search_find (ft_guid_t *guid)
{
	return dataset_lookup (searches, guid, FT_GUID_SIZE);
}

static int search_by_event (ds_data_t *key, ds_data_t *value, IFEvent *event)
{
	FTSearch *srch = value->data;

	return (srch->event == event);
}

FTSearch *ft_search_find_by_event (IFEvent *event)
{
	assert (event != NULL);
	return dataset_find (searches, DS_FIND(search_by_event), event);
}

void ft_search_disable (FTSearch *srch)
{
	srch->event = NULL;
}

/*****************************************************************************/

unsigned int ft_search_sentto (FTSearch *srch, in_addr_t to)
{
	if (!srch || to == 0)
		return 0;

	if (!srch->waiting_on)
		srch->waiting_on = dataset_new (DATASET_HASH);

	dataset_insert (&srch->waiting_on, &to, sizeof (to), "in_addr_t", 0);

	/* dont worry, the underlying dataset impl does not need to traverse */
	return dataset_length (srch->waiting_on);
}

unsigned int ft_search_rcvdfrom (FTSearch *srch, in_addr_t from)
{
	if (!srch || from == 0)
		return 0;

	dataset_remove (srch->waiting_on, &from, sizeof (from));

	return dataset_length (srch->waiting_on);
}

/*****************************************************************************/

static int browse_timeout (FTBrowse *browse)
{
	ft_browse_finish (browse);
	return FALSE;
}

static int browse_begin (FTBrowse *browse)
{
	assert (dataset_lookup (browses, browse->guid, FT_GUID_SIZE) == NULL);
	dataset_insert (&browses, browse->guid, FT_GUID_SIZE, browse, 0);

	browse->timeout =
	    timer_add (4 * MINUTES, (TimerCallback)browse_timeout, browse);

	return TRUE;
}

FTBrowse *ft_browse_new (IFEvent *event, in_addr_t user)
{
	FTBrowse *browse;

	if (!(browse = MALLOC (sizeof (FTBrowse))))
		return NULL;

	browse->event = event;
	browse->guid  = ft_guid_new ();
	browse->user  = user;

	browse_begin (browse);

	return browse;
}

static void browse_free (FTBrowse *browse)
{
	ft_guid_free (browse->guid);
	free (browse);
}

void ft_browse_finish (FTBrowse *browse)
{
	if (!browse)
		return;

#if 0
	FT->DBGFN (FT, "%s", ft_guid_fmt (browse->guid));
#endif

	timer_remove_zero (&browse->timeout);

	if (browse->event)
		FT->search_complete (FT, browse->event);

	dataset_remove (browses, browse->guid, FT_GUID_SIZE);

	browse_free (browse);
}

FTBrowse *ft_browse_find (ft_guid_t *guid, in_addr_t user)
{
	FTBrowse *browse;

	if (!(browse = dataset_lookup (browses, guid, FT_GUID_SIZE)))
		return NULL;

	if (browse->user != user)
		return NULL;

	return browse;
}

static int browse_by_event (ds_data_t *key, ds_data_t *value, IFEvent *event)
{
	FTBrowse *browse = value->data;

	return (browse->event == event);
}

FTBrowse *ft_browse_find_by_event (IFEvent *event)
{
	assert (event != NULL);
	return dataset_find (browses, DS_FIND(browse_by_event), event);
}

void ft_browse_disable (FTBrowse *browse)
{
	browse->event = NULL;
}

/*****************************************************************************/

static void fwd_free (FTSearchFwd *sfwd);

static int fwd_tick_by_addr (ds_data_t *key, ds_data_t *value, void *udata)
{
	FTSearchFwd *sfwd = value->data;
	int          ret = DS_CONTINUE;

	/* see the comment in fwd_timeout for more information about this */
	if (sfwd->tick++ > 0)
	{
		fwd_free (sfwd);
		ret |= DS_REMOVE;
	}

	return ret;
}

static int fwd_tick_by_guid (ds_data_t *key, ds_data_t *value, void *udata)
{
	Dataset     *by_addr = value->data;
	int          ret = DS_CONTINUE;

	dataset_foreach_ex (by_addr, DS_FOREACH_EX(fwd_tick_by_addr), NULL);

	if (dataset_length (by_addr) == 0)
	{
		dataset_clear (by_addr);
		ret |= DS_REMOVE;
	}

	return ret;
}

static int fwd_timeout (void *udata)
{
	/*
	 * Handle timing out each individual search forward object through
	 * iteration of them all.  We will be looking at the tick value to be
	 * greater than 1 upon entrance, which gives a range of INTERVAL through
	 * INTERVAL*2-1 for the objects life.
	 *
	 * Please note that we are deviating from the code in
	 * ::ft_search_fwd_remove as we can provide extra optimizations here to
	 * avoid any extra lookups or iteration by using the DS_REMOVE return
	 * value from the ds foreach function.
	 */
	dataset_foreach_ex (forwards, DS_FOREACH_EX(fwd_tick_by_guid), NULL);

	/* keep the timer alive forever */
	return TRUE;
}

static void fwd_insert (FTSearchFwd *sfwd, ft_guid_t *guid)
{
	Dataset     *by_addr;
	DatasetNode *by_addr_node;

	if (!forwards)
		forwards = dataset_new (DATASET_HASH);

	/* lookup the node where the by_addr dataset resides (if it's already
	 * been constructed) */
	by_addr_node = dataset_lookup_node (forwards, guid, FT_GUID_SIZE);

	/* determine whether or not we need to create the by_addr dataset and
	 * add it to the forwards table, or see if we can just use the by_addr
	 * node we looked up */
	if (by_addr_node)
	{
		if (!(by_addr = by_addr_node->value->data))
			return;

		sfwd->addr_node = by_addr_node;
	}
	else
	{
		ds_data_t dskey;
		ds_data_t dsdata;

		/* we need to create the dataset-in-dataset... */
		if (!(by_addr = dataset_new (DATASET_HASH)))
			return;

		/* for some reason this key is required to be copied into the
		 * dataset, research later. */
		ds_data_init (&dskey, guid, FT_GUID_SIZE, 0);
		ds_data_init (&dsdata, by_addr, 0, DS_NOCOPY);

		sfwd->addr_node = dataset_insert_ex (&forwards, &dskey, &dsdata);
	}

	sfwd->node =
	    dataset_insert (&by_addr, &sfwd->dst, sizeof (sfwd->dst), sfwd, 0);

	/* do not add a timer for each object because timer_add is slow as
	 * hell :\ */
	if (fwd_timeout_timer == 0)
	{
		fwd_timeout_timer =
			timer_add (5 * MINUTES, (TimerCallback)fwd_timeout, NULL);
	}
}

FTSearchFwd *ft_search_fwd_new (ft_guid_t *guid, in_addr_t src, in_addr_t dst)
{
	FTSearchFwd *sfwd;

	if (ft_search_find (guid))
	{
		FT->DBGFN (FT, "collision with locally requested search id!");
		return NULL;
	}

	if (ft_search_fwd_find (guid, dst))
	{
		char srcbuf[16];
		char dstbuf[16];

		net_ip_strbuf (src, srcbuf, sizeof (srcbuf));
		net_ip_strbuf (dst, dstbuf, sizeof (dstbuf));

#if 0
		FT->DBGFN (FT, "refusing to create forward %s <-> %s: already exists",
		           srcbuf, dstbuf);
#endif

		return NULL;
	}

	if (!(sfwd = MALLOC (sizeof (FTSearchFwd))))
		return NULL;

	sfwd->src  = src;
	sfwd->dst  = dst;

	fwd_insert (sfwd, guid);

	return sfwd;
}

/*
 * WARNING: Any changes here must be reflected in the better optimized
 * ::fwd_timeout as well.
 */
static unsigned int fwd_remove (FTSearchFwd *sfwd)
{
	Dataset     *by_addr;
	unsigned int rem;

	assert (sfwd->addr_node != NULL);
	assert (sfwd->node != NULL);

	by_addr = sfwd->addr_node->value->data;
	assert (by_addr != NULL);

	/* remove our host address so that we may determine how many are left */
	dataset_remove_node (by_addr, sfwd->node);

	/* get the number of forwards left by this guid */
	rem = dataset_length (by_addr);

	/* if there are no more forwards by this quid, remove the by_addr node
	 * from the main forwards table */
	if (rem == 0)
	{
		dataset_remove_node (forwards, sfwd->addr_node);
		dataset_clear (by_addr);
	}

	return rem;
}

static void fwd_free (FTSearchFwd *sfwd)
{
	if (!sfwd)
		return;

	/* we used to do more, but we optimized it away :) */
	free (sfwd);
}

unsigned int ft_search_fwd_finish (FTSearchFwd *sfwd)
{
	unsigned int rem;

	if (!sfwd)
		return 0;

	rem = fwd_remove (sfwd);
	fwd_free (sfwd);

	return rem;
}

FTSearchFwd *ft_search_fwd_find (ft_guid_t *guid, in_addr_t dst)
{
	Dataset *by_addr;

	if (!(by_addr = dataset_lookup (forwards, guid, FT_GUID_SIZE)))
		return NULL;

	return dataset_lookup (by_addr, &dst, sizeof (dst));
}
