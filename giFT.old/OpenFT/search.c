/*
 * search.c
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

#include <ctype.h>

#include "openft.h"

#include "http.h"
#include "html.h"
#include "search.h"

#include "parse.h"

#include "share.h"

/*****************************************************************************/

/**/extern HashTable  *ft_shares;
/**/extern Connection *ft_self;

unsigned long search_unique_id = 0;

/*****************************************************************************/

typedef int (*SearchFunc) (FileShare *file, Dataset *dataset);

static HashTable *active_searches = NULL;

/* for optimization purposes, a lot better than using a dataset */
struct _match_data
{
	List      *results;
	size_t     size;
	SearchFunc sfunc;
	Dataset   *dataset;
};

/*****************************************************************************/

#define MAX_RESULTS 1000
#define TOKEN_PUNCT ",`'!?*"
#define TOKEN_DELIM "\\/ _-.[]()\t"

/*****************************************************************************/

static void token_remove_punct (char *token)
{
	char *ptr;

	if (!token[0])
		return;

	while ((ptr = string_sep_set (&token, TOKEN_PUNCT)))
	{
		if (!token)
			continue;

		strmove (token - 1, token);
		token--;
	}
}

/*
 * tokens are delimited by the set TOKEN_DELIM
 * tokens are stripped of all punctuation
 */
ft_uint32 *search_tokenize (char *string)
{
	char      *token, *string0;
	ft_uint32 *tokens;
	int        tok = 0, tok_alloc = 32;

	if (!string)
		return NULL;

	/* allocate the initial buffer */
	tokens = malloc (tok_alloc * sizeof (ft_uint32));

	string0 = string = strdup (string);

	while ((token = string_sep_set (&string, TOKEN_DELIM)))
	{
		/* leave room for the null termination */
		if (tok >= (tok_alloc - 1))
		{
			tok_alloc *= 2;
			tokens = realloc (tokens, tok_alloc * sizeof (ft_uint32));
			continue;
		}

		token_remove_punct (token);

		if (*token)
			tokens[tok++] = hash_string (string_lower (token));
	}

	free (string0);

	/* null terminate */
	tokens[tok++] = 0;

	/* this is evil and wrong, but tightens ram usage */
	tokens = realloc (tokens, tok * sizeof (ft_uint32));

	return tokens;
}

/*****************************************************************************/

/* readability or optimization...sigh...someone make this difficult
 * decision for me */
static int match_filename (FileShare *file, Dataset *dataset)
{
	OpenFT_Share *openft;
	char         *realm;
	ft_uint32    *tokens;
	ft_uint32    *qtokens, *etokens;
	ft_uint32    *ptr;

	if (!dataset)
		return FALSE;

	realm   = dataset_lookup (dataset, "realm");
	qtokens = dataset_lookup (dataset, "qtokens");
	etokens = dataset_lookup (dataset, "etokens");
	/* TODO -- handle the rest of the dataset */

	if (!file)
	{
		free (qtokens);
		free (etokens);
		return FALSE;
	}

	/* quick check to make sure we exist in the proper realm */
	if (realm)
	{
		if (strncmp (file->type, realm, strlen (realm)))
			return FALSE;
	}

	openft = dataset_lookup (file->data, "OpenFT");

	/* just in case we haven't tokenized this file yet */
	if (!openft->tokens)
		openft->tokens = search_tokenize (file->path);

	/* check exclusion first */
	for (ptr = etokens; ptr && *ptr; ptr++)
	{
		for (tokens = openft->tokens; *tokens; tokens++)
			if (*tokens == *ptr)
				return FALSE;
	}

	/* TODO - optimize this again ;) */
	for (ptr = qtokens; ptr && *ptr; ptr++)
	{
		int matched = 0;

		/* 3:27am...is this correct? ... oh my god I can't feel my finger...
		 * -- monitor off -- */
		for (tokens = openft->tokens; *tokens; tokens++)
		{
			if (*tokens == *ptr)
				matched = 1;
		}

		if (!matched)
			return FALSE;
	}

	return TRUE;
}

static int match_md5 (FileShare *file, Dataset *dataset)
{
	char *md5;

	md5 = dataset_lookup (dataset, "query");

	if (!file)
		return FALSE;

	return (strcmp (file->md5, md5) == 0);
}

static int match_host (FileShare *file, Dataset *dataset)
{
	OpenFT_Share *openft;
	unsigned long ip;

	ip = (unsigned long) dataset_lookup (dataset, "query");

	if (!file)
		return FALSE;

	openft = dataset_lookup (file->data, "OpenFT");

	return (openft->host == ip);
}

/*****************************************************************************/

static SearchFunc search_match_func (Dataset **dataset, int search_type,
                                     char *query, char *exclude, char *realm,
                                     size_t size_min, size_t size_max,
                                     size_t kbps_min, size_t kbps_max)
{
	SearchFunc match_func = NULL;

	if (search_type & SEARCH_FILENAME)
	{
		match_func = (SearchFunc) match_filename;

		dataset_insert (*dataset, "query",    query);
		dataset_insert (*dataset, "exclude",  exclude);
		dataset_insert (*dataset, "qtokens",  search_tokenize (query));
		dataset_insert (*dataset, "etokens",  search_tokenize (exclude));
		dataset_insert (*dataset, "realm",    realm);
		dataset_insert (*dataset, "size_min", (void *) size_min);
		dataset_insert (*dataset, "size_max", (void *) size_max);
		dataset_insert (*dataset, "kbps_min", (void *) kbps_min);
		dataset_insert (*dataset, "kbps_max", (void *) kbps_max);
	}
	else if (search_type & SEARCH_MD5)
	{
		match_func = (SearchFunc) match_md5;

		dataset_insert (*dataset, "query",    query);
	}
	else if (search_type & SEARCH_HOST)
	{
		match_func = (SearchFunc) match_host;

		dataset_insert (*dataset, "query",    (void *) inet_addr (query));
	}

	return match_func;
}

/*****************************************************************************/

static int key_as_md5 (unsigned long key, FileShare *file,
					   struct _match_data *mdata)
{
	if (mdata->size >= MAX_RESULTS)
		return FALSE;

	if (!file)
		return TRUE;

	/* match the file */
	if ((*(mdata->sfunc)) (file, mdata->dataset))
	{
		openft_share_ref (file);
		mdata->results = list_append (mdata->results, file);
		mdata->size++;
	}

	return TRUE;
}

static int key_as_host (unsigned long key, Dataset *dataset,
						struct _match_data *mdata)
{
	if (mdata->size >= MAX_RESULTS)
		return FALSE;

	/* real search func */
	hash_table_foreach (dataset, (HashFunc) key_as_md5, mdata);

	return TRUE;
}

/* NOTE: ft_search returns a newly allocated list, however, the data must be
 * unref'd, not free'd! */
List *ft_search (size_t *r_size, int search_type,
                 char *query, char *exclude, char *realm,
                 size_t size_min, size_t size_max,
                 size_t kbps_min, size_t kbps_max)
{
	struct      _match_data mdata;
	int         local = 0;

	/* this cant happen */
	if (!query)
		return NULL;

	/* init search data */
	memset (&mdata, 0, sizeof (mdata));

	if (search_type & SEARCH_LOCAL)
	{
		search_type &= ~SEARCH_LOCAL;
		local = 1;
	}

	/* create the search match data and return the function to supply it to */
	if (!(mdata.sfunc = search_match_func (&(mdata.dataset), search_type,
	                                       query, exclude, realm,
	                                       size_min, size_max,
	                                       kbps_min, kbps_max)))
	{
		/* fuck. */
		return NULL;
	}

	/* start looking */
	hash_table_foreach (ft_shares, (HashFunc) key_as_host, &mdata);

#if 0
	/* sigh, optimize this with our own special data structure later */
	for (ptr = ft_shares; ptr; ptr = list_next (ptr))
	{
		FileShare    *file = ptr->data;

		if (results_size >= MAX_RESULTS)
			break;

		if (!file)
			continue;

		if ((*match_func) (file, dataset))
		{
			openft_share_ref (file);
			results = list_append (results, file);
			results_size++;
		}
	}
#endif

	/* search the files this node is sharing ... */
	if (local && mdata.size < MAX_RESULTS)
		share_foreach ((HashFunc) key_as_md5, &mdata);

	/* cleanup */
	(*(mdata.sfunc)) (NULL, mdata.dataset);
	dataset_clear (mdata.dataset);

	if (r_size)
		*r_size = mdata.size;

	return mdata.results;
}

/*****************************************************************************/

Search *search_new (IFEventID id, int search_type, char *query, char *exclude,
					char *realm, size_t size_min, size_t size_max,
					size_t kbps_min, size_t kbps_max)
{
	Search *search;

	search = hash_table_lookup (active_searches, id);

	if (!search)
	{
		search = malloc (sizeof (Search));

		search->id       = id;
		search->type     = search_type;
		search->query    = STRDUP (query);
		search->exclude  = STRDUP (exclude);
		search->realm    = STRDUP (realm);
		search->size_min = size_min;
		search->size_max = size_max;
		search->kbps_min = kbps_min;
		search->kbps_max = kbps_max;
		search->qtokens  = search_tokenize (query);
		search->etokens  = search_tokenize (query);
		search->ref      = 0;

		if (!active_searches)
			active_searches = hash_table_new ();

		hash_table_insert (active_searches, id, search);
	}

	/* number of hosts that must return results */
	search->ref++;

	return search;
}

static void search_free (Search *search)
{
	assert (search);

	free (search->query);
	free (search->exclude);
	free (search->realm);
	free (search->qtokens);
	free (search->etokens);
	free (search);
}

void search_remove (IFEventID id)
{
	Search *search;

	TRACE (("removing search id %lu", id));

	search = hash_table_lookup (active_searches, id);
	hash_table_remove (active_searches, id);

	/* TODO - temporary!!!!! */
	if_event_remove (id);

	search_free (search);
}

/*****************************************************************************/

/* daemon -> user interface search result */
void search_reply (IFEventID id, Connection *search_node, FileShare *file)
{
	OpenFT_Share *openft;
	SearchFunc m_func;
	Dataset   *dataset = NULL;
	Search    *search;
	char      *host;
	char      *href;
	char      *file_enc;

	if (!(search = hash_table_lookup (active_searches, id)))
		return;

	/* end of search */
	if (!file)
	{
		search->ref--;

#if 0
		TRACE (("dec: search->ref = %i", search->ref));
#endif

		if (!search->ref)
			search_remove (search->id);

		return;
	}

	openft = dataset_lookup (file->data, "OpenFT");

	/* the search result is firewalled as is this local node, displaying the
	 * result is futile and annoying */
	if (openft->port == 0 && NODE (ft_self)->port == 0)
		return;

	/* verify that this wasnt a faulty search */
	if ((m_func = search_match_func (&dataset, search->type, search->query,
	                                 search->exclude, search->realm,
	                                 search->size_min, search->size_max,
	                                 search->kbps_min, search->kbps_max)))
	{
		int ret = ((*m_func) (file, dataset));

		/* failed match, ignore this result */
		if (!ret)
			return;
	}

	/* otherwise, this is a normal search reply, send it back to the
	 * interface protocol */

	file_enc = url_encode (file->path);
	href = malloc (2048 + strlen (file_enc));

	host = STRDUP (net_ip_str (openft->host));

	/* if the file was shared from a firewalled host, we need to construct
	 * a special URL with the search node */
	if (openft->port == 0 && search_node)
	{
		sprintf (href,
				 "OpenFT://%s:%hu/?listen_port=%hu&request_host=%s&"
				 "request_file=%s",
				 net_ip_str (NODE (search_node)->ip),
				 NODE (search_node)->http_port, NODE (ft_self)->http_port,
				 host, file_enc);
	}
	else
	{
		sprintf (href, "OpenFT://%s:%hu%s", net_ip_str (openft->host),
				 openft->http_port, file_enc);
	}

	free (file_enc);

	if_event_reply (id, "item",
	                "id=i",   search->id,
	                "user=s", host,
	                "node=s", net_ip_str (NODE (search_node)->ip),
	                "href=s", href,
	                "size=i", file->size,
	                "hash=s", file->md5,
	                NULL);

	free (href);
	free (host);
}
