/*
 * $Id: ft_search_obj.c,v 1.2 2003/05/05 09:49:11 jasta Exp $
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

/*****************************************************************************/

static Dataset *searches = NULL;       /* direct local searches */
static Dataset *browses  = NULL;       /* direct local browses */
static Dataset *forwards = NULL;       /* forwarded search queries */

/*****************************************************************************/

static void set_params (FTSearchParams *dst, FTSearchType type, char *realm,
                        char *query, char *exclude)
{
	assert (type != 0x00);
	assert (query != NULL);

	dst->type    = type;
	dst->realm   = STRDUP (realm);
	dst->query   = STRDUP (query);
	dst->exclude = STRDUP (exclude);
	dst->qtokens = ft_search_tokenize (query);
	dst->etokens = ft_search_tokenize (exclude);
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

FTSearch *ft_search_new (IFEvent *event, FTSearchType type, char *realm,
                         char *query, char *exclude)
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

static void free_params (FTSearchParams *params)
{
	free (params->realm);
	free (params->query);
	free (params->exclude);
	free (params->qtokens);
	free (params->etokens);
}

static void search_free (FTSearch *srch)
{
	ft_guid_free (srch->guid);
	free_params (&srch->params);
	dataset_clear (srch->waiting_on);

	free (srch);
}

void ft_search_finish (FTSearch *srch)
{
	if (!srch)
		return;

	FT->DBGFN (FT, "%s", ft_guid_fmt (srch->guid));

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

static int search_by_event (Dataset *d, DatasetNode *node, IFEvent *event)
{
	FTSearch *srch = node->value;

	return (srch->event == event);
}

FTSearch *ft_search_find_by_event (IFEvent *event)
{
	assert (event != NULL);
	return dataset_find (searches, DATASET_FOREACH(search_by_event), event);
}

void ft_search_disable (FTSearch *srch)
{
	srch->event = NULL;
}

/*****************************************************************************/

unsigned int ft_search_sentto (FTSearch *srch, FTNode *node)
{
	if (!srch || !node)
		return 0;

	if (!srch->waiting_on)
		srch->waiting_on = dataset_new (DATASET_HASH);

	dataset_insert (&srch->waiting_on, &node->ip, sizeof (node->ip),
	                "in_addr_t", 0);

	return dataset_length (srch->waiting_on);
}

unsigned int ft_search_rcvdfrom (FTSearch *srch, FTNode *node)
{
	if (!srch || !node)
		return 0;

	dataset_remove (srch->waiting_on, &node->ip, sizeof (node->ip));

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

	FT->DBGFN (FT, "%s", ft_guid_fmt (browse->guid));

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

static int browse_by_event (Dataset *d, DatasetNode *node, IFEvent *event)
{
	FTBrowse *browse = node->value;

	return (browse->event == event);
}

FTBrowse *ft_browse_find_by_event (IFEvent *event)
{
	assert (event != NULL);
	return dataset_find (browses, DATASET_FOREACH(browse_by_event), event);
}

void ft_browse_disable (FTBrowse *browse)
{
	browse->event = NULL;
}

/*****************************************************************************/

struct fwdlookup
{
	ft_guid_t guid[FT_GUID_SIZE];
	in_addr_t dst;
};

static struct fwdlookup *fwd_lookup_data (ft_guid_t *guid, in_addr_t dst)
{
	static struct fwdlookup data;

	assert (guid != NULL);

	memcpy (data.guid, guid, FT_GUID_SIZE);
	data.dst = dst;

	return &data;
}

static int fwd_timeout (FTSearchFwd *sfwd)
{
	ft_search_fwd_finish (sfwd);
	return FALSE;
}

static void fwd_insert (FTSearchFwd *sfwd)
{
	struct fwdlookup *data;

	if (!forwards)
		forwards = dataset_new (DATASET_HASH);

	data = fwd_lookup_data (sfwd->guid, sfwd->dst);
	dataset_insert (&forwards, data, sizeof (struct fwdlookup), sfwd, 0);

	sfwd->timeout =
	    timer_add (5 * MINUTES, (TimerCallback)fwd_timeout, sfwd);
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

		FT->DBGFN (FT, "refusing to create forward %s <-> %s: already exists",
		           srcbuf, dstbuf);

		return NULL;
	}

	if (!(sfwd = MALLOC (sizeof (FTSearchFwd))))
		return NULL;

	sfwd->guid = ft_guid_dup (guid);
	sfwd->src  = src;
	sfwd->dst  = dst;

	fwd_insert (sfwd);

	return sfwd;
}

void ft_search_fwd_finish (FTSearchFwd *sfwd)
{
	struct fwdlookup *data;

	if (!sfwd)
		return;

	FT->DBGFN (FT, "%s", ft_guid_fmt (sfwd->guid));

	data = fwd_lookup_data (sfwd->guid, sfwd->dst);
	dataset_remove (forwards, data, sizeof (struct fwdlookup));

	ft_guid_free (sfwd->guid);
	free (sfwd);
}

FTSearchFwd *ft_search_fwd_find (ft_guid_t *guid, in_addr_t dst)
{
	struct fwdlookup *data;

	data = fwd_lookup_data (guid, dst);
	return dataset_lookup (forwards, data, sizeof (struct fwdlookup));
}
