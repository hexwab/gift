/*
 * $Id: gt_search_exec.c,v 1.14 2003/06/07 05:24:14 hipnod Exp $
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

#include "gt_gnutella.h"

#include "gt_search.h"
#include "gt_search_exec.h"

#include "gt_share.h"       /* gt_share_local_lookup_by_urn */
#include "gt_share_file.h"

#include "sha1.h"           /* sha1_string */

/******************************************************************************/

#define MAX_RESULTS       200

/******************************************************************************/

GtTokenSet *gt_token_set_new ()
{
	GtTokenSet *ts;

	if (!(ts = MALLOC (sizeof (GtTokenSet))))
		return NULL;

	return ts;
}

void gt_token_set_free (GtTokenSet *ts)
{
	if (!ts)
		return;

	free (ts->data);
	free (ts);
}

void gt_token_set_append (GtTokenSet *ts, uint32_t token)
{
	if (ts->len >= ts->data_len)
	{
		uint32_t *new_tokens;

		ts->data_len += 8;
		new_tokens = realloc (ts->data, ts->data_len * sizeof (uint32_t));

		assert (new_tokens != NULL);
		ts->data = new_tokens;
	}

	ts->data[ts->len++] = token;
}

/******************************************************************************/

static List *search_by_hash (unsigned char *hash)
{
	FileShare *file;
	char      *str;
	char      *urn;

	/* ugly that this is so sha1 specific:need a urn abstraction desperately */
	if (!(str = sha1_string (hash)))
		return NULL;

	urn = stringf_dup ("urn:sha1:%s", str);
	free (str);

	if (!(file = gt_share_local_lookup_by_urn (urn)))
	{
		free (urn);
		return NULL;
	}

	GT->DBGFN (GT, "Wuh-HOO! Answered a query-by-hash (%s) for (%s)",
	           urn, share_get_hpath (file));
	
	free (urn);

	return list_append (NULL, file);
}

static int search_slowly (ds_data_t *key, ds_data_t *value, void **cmp)
{
	GtTokenSet *tokens       = cmp[0];
	List      **results      = cmp[1];
	int        *max_results  = cmp[2];
	int        *count        = cmp[3];
	FileShare  *file         = value->data;
	GtShare    *share;
	int         i, j;
	int         matched;
	GtTokenSet *cmp_tokens;

	if (*count >= *max_results)
		return DS_BREAK;

	/* hipnod: this used to be equiv to DS_BREAK, i changed to DS_CONTINUE */
	if (!(share = share_get_udata (file, gnutella_p->name)))
		return DS_CONTINUE;

	cmp_tokens = share->tokens;
	matched    = 0;

	for (i = 0; i < tokens->len; i++)
	{
		int old_matched = matched;

		for (j = 0; j < cmp_tokens->len; j++)
		{
			if (tokens->data[i] == cmp_tokens->data[j])
			{
				matched++;
				break;
			}
		}

		if (matched == old_matched)
			break;
	}

	if (matched == tokens->len)
	{
		*results = list_prepend (*results, file);
		(*count)++;
	}

	return DS_CONTINUE;
}

List *gt_search_exec (char *query, GtSearchType type, void *extended,
                      uint8_t ttl, uint8_t hops)
{
	List          *results     = NULL;
	GtTokenSet    *tokens;
	int            max_results = MAX_RESULTS;
	int            count       = 0;
	void          *cmp[4];
	StopWatch     *sw;
	double         elapsed;
	unsigned char *hash;

	if (!query || string_isempty (query))
		return NULL;

	if (!(tokens = gt_share_tokenize (query)))
		return NULL;

	cmp[0] = tokens;
	cmp[1] = &results;
	cmp[2] = &max_results;
	cmp[3] = &count;

	sw = stopwatch_new (TRUE);
	hash = extended;

	if (type == GT_SEARCH_KEYWORD)
		share_foreach (DS_FOREACH_EX(search_slowly), cmp);
	else if (type == GT_SEARCH_HASH)
		results = search_by_hash (hash);
	else
		assert (0);

	elapsed = stopwatch_free_elapsed (sw);

	/* Calling LOG_RESULTS here uses a system call each time, ah well */
	if (LOG_RESULTS)
		GT->dbg (GT, "results: [%03d] [%d|%d] %.04fs (%s)", count,
		         ttl, hops, elapsed, query);

	gt_token_set_free (tokens);

	return results;
}
