/*
 * $Id: gt_search_exec.c,v 1.9 2003/05/04 07:43:43 hipnod Exp $
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

#include "gt_search_exec.h"
#include "gt_share_file.h"

#include "gt_search_exec.h"

/******************************************************************************/

#define MAX_RESULTS       200

#define LOG_RESULTS      config_get_int (gt_conf, "search/log_results=0")

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

static int search_slowly (Dataset *d, DatasetNode *node, void **cmp)
{
	GtTokenSet *tokens       = cmp[0];
	List      **results      = cmp[1];
	int        *max_results  = cmp[2];
	int        *count        = cmp[3];
	FileShare  *file         = node->value;
	Gt_Share   *share;
	int         i, j;
	int         matched;
	GtTokenSet *cmp_tokens;

	if (*count >= *max_results)
		return FALSE;

	if (!(share = share_lookup_data (file, gnutella_p->name)))
		return FALSE;

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
		gt_share_ref (file);
		*results = list_prepend (*results, file);
		(*count)++;
	}

	return TRUE;
}

List *gt_search_exec (char *query, uint8_t ttl, uint8_t hops)
{
	List      *results     = NULL;
	GtTokenSet *tokens;
	int        max_results = MAX_RESULTS;
	int        count       = 0;
	void      *cmp[4];
	StopWatch *sw;
	double     elapsed;

	if (!query || string_isempty (query))
		return NULL;

	if (!(tokens = gt_share_tokenize (query)))
		return NULL;

	cmp[0] = tokens;
	cmp[1] = &results;
	cmp[2] = &max_results;
	cmp[3] = &count;

	sw = stopwatch_new (TRUE);

	share_foreach (DATASET_FOREACH (search_slowly), cmp);

	elapsed = stopwatch_free_elapsed (sw);
	
	/* Calling LOG_RESULTS here uses a system call each time, ah well */
	if (LOG_RESULTS)
		GT->dbg (GT, "results: [%03d] [%d|%d] %.04fs (%s)", count, 
		         ttl, hops, elapsed, query);

	gt_token_set_free (tokens);

	return results;
}
