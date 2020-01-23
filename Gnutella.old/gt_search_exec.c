/*
 * $Id: gt_search_exec.c,v 1.18 2003/07/09 09:31:52 hipnod Exp $
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

#include "gt_query_route.h" /* QRP_DELIMITERS */
#include "gt_share.h"       /* gt_share_local_lookup_by_urn */
#include "gt_share_file.h"

#include "sha1.h"           /* sha1_string */
#include "trie.h"

/******************************************************************************/

#define MAX_RESULTS       200

/******************************************************************************/

static Trie *gt_search_trie;

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

static List *by_hash (unsigned char *hash, size_t *n)
{
	FileShare *file;
	char      *str;
	char      *urn;

	*n = 0;

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
	
	*n = 1;
	free (urn);

	return list_append (NULL, file);
}

/******************************************************************************/

/* some defines for list_find_custom that work like DS_BREAK, etc. */
#define LIST_BREAK     0
#define LIST_CONTINUE  1

static int search_slowly (Share *file, void **cmp)
{
	GtTokenSet *tokens       = cmp[0];
	List      **results      = cmp[1];
	int        *max_results  = cmp[2];
	int        *count        = cmp[3];
	GtShare    *share;
	int         i, j;
	int         matched;
	GtTokenSet *cmp_tokens;

	if (*count >= *max_results)
		return LIST_BREAK;

	if (!(share = share_get_udata (file, gnutella_p->name)))
		return LIST_CONTINUE;

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

	if (matched >= tokens->len)
	{
		*results = list_prepend (*results, file);
		(*count)++;
	}

	return LIST_CONTINUE;
}

/*
 * Find the smallest list corresponding to a word in the query
 * that is in the trie of shares.
 */
static List *find_smallest (char *query)
{
	char    *str;
	char    *str0;
	List    *smallest      = NULL;
	size_t   smallest_size = 0;
	char    *tok;
	size_t   size;

	if (!(str = str0 = STRDUP (query)))
		return NULL;

	string_lower (str);

	while ((tok = string_sep_set (&str, QRP_DELIMITERS)))
	{
		List *list;

		if (string_isempty (tok))
			continue;

		if (!(list = trie_lookup (gt_search_trie, tok)))
		{
			/* no shares have this token: therefore no match */
			smallest      = NULL;
			smallest_size = 0;
			break;
		}

		if ((size = list_length (list)) < smallest_size || smallest_size == 0)
		{
			/* keep this one */
			smallest_size = size;
			smallest      = list;
		}
	}

	free (str0);

	if (LOG_RESULTS)
		GT->DBGFN (GT, "scanning list of %d size", smallest_size);

	return smallest;
}

/*
 * Look up the list of all shares for each word in the query in the 
 * Trie of all local shares. Find the keyword with the smallest
 * list, then iterate the list looking for shares.
 */
static List *by_keyword (char *query, size_t max_results, size_t *n)
{
	GtTokenSet *tokens;
	List       *results  = NULL;
	List       *smallest;
	void       *cmp[4];

	if (!query || string_isempty (query))
		return NULL;

	if (!(tokens = gt_share_tokenize (query)))
		return NULL;

	cmp[0] = tokens;
	cmp[1] = &results;
	cmp[2] = &max_results;
	cmp[3] = n;

	smallest = find_smallest (query);
	list_find_custom (smallest, cmp, (ListForeachFunc)search_slowly);

	gt_token_set_free (tokens);
	return results;
}

List *gt_search_exec (char *query, GtSearchType type, void *extended,
                      uint8_t ttl, uint8_t hops)
{
	List          *results;
	int            max_res     = MAX_RESULTS;
	StopWatch     *sw;
	double         elapsed;
	unsigned char *hash;
	size_t         n           = 0;

	sw = stopwatch_new (TRUE);
	hash = extended;

	switch (type)
	{
	 case GT_SEARCH_KEYWORD: results = by_keyword (query, max_res, &n);  break;
	 case GT_SEARCH_HASH:    results = by_hash    (hash, &n);            break;
	 default: abort ();
	}

	elapsed = stopwatch_free_elapsed (sw);

	if (LOG_RESULTS)
	{
		GT->dbg (GT, "results: [%03d] [%d|%d] %.06fs (%s)", n,
		         ttl, hops, elapsed, query);
	}

	return results;
}

/******************************************************************************/

static void add_share (Trie *trie, char *tok, Share *share)
{
	List *list;

	list = trie_lookup (trie, tok);

	/* the share may already be in the list if the filename has
	 * two words that are the same */
	if (list_find (list, share))
		return;

	list = list_prepend (list, share);

	trie_remove (trie, tok);
	trie_insert (trie, tok, list);
}

static void del_share (Trie *trie, char *tok, Share *share)
{
	List *list;

	list = trie_lookup (trie, tok);
	list = list_remove (list, share);

	trie_remove (trie, tok);

	if (!list)
		return;

	trie_insert (trie, tok, list);
}

static void search_trie_change (Trie *trie, Share *share, BOOL add)
{
	char *tok;
	char *str0, *str;

	if (!(str0 = str = STRDUP (share_get_hpath (share))))
		return;

	string_lower (str);

	while ((tok = string_sep_set (&str, QRP_DELIMITERS)))
	{
		if (string_isempty (tok))
			continue;

		if (add)
			add_share (trie, tok, share);
		else
			del_share (trie, tok, share);
	}

	free (str0);
}

void gt_search_exec_add (Share *share)
{
	search_trie_change (gt_search_trie, share, TRUE);
}

void gt_search_exec_remove (Share *share)
{
	search_trie_change (gt_search_trie, share, FALSE);
}

void gt_search_exec_sync (void)
{
#if 0
	printf ("SHARE TRIE:\n");
	trie_print (gt_search_trie);
#endif
}

void gt_search_exec_init (void)
{
	gt_search_trie = trie_new ();
}

void gt_search_exec_cleanup (void)
{
	trie_free (gt_search_trie);
	gt_search_trie = NULL;
}
