/*
 * ft_search_exec.c
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

#include "ft_openft.h"

#include "ft_search.h"
#include "ft_search_exec.h"

#include "stopwatch.h"
#include "md5.h"
#include "meta.h"

#include <errno.h>

/*****************************************************************************/

/*
 * Here's how child searches work, to spare you from reading this awful
 * awful code :)
 *
 * FT_SEARCH_MD5
 *
 * Loops through all share hosts (see ft_shost.c) executing a single
 * DB->get in the primary database.  The 16 byte MD5 sum is held as the key
 * without duplicates possible.
 *
 * FT_SEARCH_FILENAME
 *
 * Currently loops through all share hosts objects exactly as FT_SEARCH_MD5
 * does (this is subject to change, as is FT_SEARCH_MD5 for that matter),
 * however the searches are executed with a lot more DB overhead.  First,
 * each query token is assigned to its own cursor which represents a lookup
 * in the secondary database (4 byte token keys, duplicates allowed and
 * expected).  We then loop through each data item at the first cursor
 * (16 byte MD5s) walking along all subsequent cursors attempting to identify
 * a match.  Each successful result (that is, a single md5 sum which exists
 * in all cursor sets) is added to its own list and may be considered a
 * "dirty match".  After all dirty matches are allocated, we then look them
 * up in the primary database and further verify the match by applying realm
 * and eventually meta data checks.
 *
 * FT_SEARCH_HOST
 *
 * Deprecated and no longer used for remote searches.
 */

/*****************************************************************************/

struct _sdata;
typedef int (*SearchFunc) (struct _sdata *sdata, FileShare *File);

struct _sdata
{
	FTSHost      *shost;               /* cached shost object during
	                                    * iteration */
	List         *results;
	size_t        matches;

	FTSearchType  type;
	SearchFunc    sfunc;

	/* FT_SEARCH_LOCAL */
	unsigned long avail;

	/* FT_SEARCH_FILENAME */
	char      *f_query;
	char      *f_exclude;
	ft_uint32 *f_qtokens;
	ft_uint32 *f_etokens;
	char      *f_realm;

	/* FT_SEARCH_MD5 */
	unsigned char *m_query;

	/* FT_SEARCH_HOST */
	unsigned long h_query;
};

/* recalculated before every search */
static unsigned long max_results = 0;

/*****************************************************************************/

/*
 * COMPARE FUNCTIONS
 *
 * NOTE:
 * These are used primarily on the user node side to verify incoming search
 * results
 */

static int cmp_filename (struct _sdata *sdata, FileShare *file)
{
	FTShare  *share;
	ft_uint32 *tptr;
	ft_uint32 *ptr;
	int        ret = TRUE;

	if (sdata->f_realm)
	{
		if (strncmp (file->mime, sdata->f_realm, strlen (sdata->f_realm)))
			return FALSE;
	}

	if (!(share = share_lookup_data (file, "OpenFT")))
		return FALSE;

	assert (share->tokens != NULL);

	/* check exclusion first */
	for (ptr = sdata->f_etokens; ptr && *ptr; ptr++)
	{
		for (tptr = share->tokens; *tptr; tptr++)
		{
			if (*tptr != *ptr)
				continue;

			/* matched exclusion, bail out */
			return FALSE;
		}
	}

	/* check main token list */
	for (ptr = sdata->f_qtokens; ptr && *ptr; ptr++)
	{
		int matched = FALSE;

		for (tptr = share->tokens; *tptr; tptr++)
		{
			if (*tptr == *ptr)
			{
				matched = TRUE;
				break;
			}
		}

		if (!matched)
			return FALSE;
	}

	return ret;
}

static int cmp_md5 (struct _sdata *sdata, FileShare *file)
{
	ShareHash *sh;

	if (!(sh = share_hash_get (file, "MD5")))
		return FALSE;

	return (memcmp (sh->hash, sdata->m_query, sh->len) == 0);
}

static int cmp_host (struct _sdata *sdata, FileShare *file)
{
	/* non-local search */
	if (sdata->shost)
		return (sdata->shost->host == sdata->h_query);

	/*
	 * local search
	 *
	 * NOTE:
	 * we don't have the ip address of the local node, therefore we cannot
	 * verify that this was in fact a match
	 *
	 * TODO:
	 * search by host should not be done like this...sigh
	 */
	return FALSE;
}

/*****************************************************************************/

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

		string_move (token - 1, token);
		token--;
	}
}

static ft_uint32 tokenize_str (char *str)
{
	ft_uint32 hash = 0;

	if (!str)
		return 0;

	if (!(hash = *str++))
		return 0;

	while (*str)
		hash = (hash << 5) - hash + *str++;

	return hash;
}

static int cmp_token (const void *a, const void *b)
{
	ft_uint32 *ta = (ft_uint32 *) a;
	ft_uint32 *tb = (ft_uint32 *) b;

	if (*ta < *tb)
		return -1;

	if (*ta > *tb)
		return 1;

	return 0;
}

static ft_uint32 *remove_dups (ft_uint32 *tokens, int num)
{
	ft_uint32 *ntokens;
	ft_uint32  lt = 0;
	int        nt;
	int        t;

	if (num <= 0)
		return tokens;

	/* sort first */
	qsort (tokens, num, sizeof (ft_uint32), cmp_token);

	/* allocate new token list */
	if (!(ntokens = malloc (sizeof (ft_uint32) * (num + 1))))
		return tokens;

	for (t = 0, nt = 0; t < num && nt < num; t++)
	{
		if (lt && tokens[t] == lt)
			continue;

		lt = tokens[t];
		ntokens[nt++] = tokens[t];
	}

	ntokens[nt++] = 0;

	free (tokens);
	return ntokens;
}

/*
 * tokens are delimited by the set TOKEN_DELIM
 * tokens are stripped by the set TOKEN_PUNCT
 */
ft_uint32 *ft_search_tokenize (char *string)
{
	char      *token, *string0;
	ft_uint32 *tokens;
	int        tok = 0, tok_alloc = 32;

	if (!string)
		return NULL;

	/* allocate the initial buffer */
	if (!(tokens = malloc (tok_alloc * sizeof (ft_uint32))))
		return NULL;

	string0 = string = strdup (string);

	while ((token = string_sep_set (&string, TOKEN_DELIM)))
	{
		/* leave room for the null termination */
		if (tok >= (tok_alloc - 1))
		{
			tok_alloc *= 2;
			tokens = realloc (tokens, tok_alloc * sizeof (ft_uint32));
		}

		token_remove_punct (token);

		if (*token)
			tokens[tok++] = tokenize_str (string_lower (token));
	}

	free (string0);

	/* null terminate */
	tokens[tok++] = 0;

	/* sort and remove duplicates */
	tokens = remove_dups (tokens, tok - 1);

	return tokens;
}

/*****************************************************************************/

static int fill_sdata (struct _sdata *sdata, FTSearchType type, char *realm,
                       void *query, void *exclude)
{
	if (!query)
		return FALSE;

	memset (sdata, 0, sizeof (struct _sdata));

	sdata->type = type;

	if (sdata->type & FT_SEARCH_FILENAME)
	{
		ft_uint32 *qtokens;
		ft_uint32 *etokens;

		/* hidden searches are pretokenized */
		if (sdata->type & FT_SEARCH_HIDDEN)
		{
			qtokens = query;
			etokens = exclude;

			query   = NULL;
			exclude = NULL;
		}
		else
		{
			qtokens = ft_search_tokenize (query);
			etokens = ft_search_tokenize (exclude);

			if (!qtokens)
			{
				free (qtokens);
				free (etokens);
				return FALSE;
			}
		}

		sdata->sfunc     = (SearchFunc) cmp_filename;
		sdata->f_query   = query;
		sdata->f_exclude = exclude;
		sdata->f_qtokens = qtokens;
		sdata->f_etokens = etokens;
		sdata->f_realm   = realm;
	}
	else if (sdata->type & FT_SEARCH_MD5)
	{
		sdata->sfunc = (SearchFunc) cmp_md5;

		if (!(sdata->m_query = md5_bin (query)))
			return FALSE;
	}
	else if (sdata->type & FT_SEARCH_HOST)
	{
		sdata->sfunc = (SearchFunc) cmp_host;

		if (!(sdata->h_query = net_ip (query)))
			return FALSE;
	}

	return (sdata->sfunc ? TRUE : FALSE);
}

static void clear_sdata (struct _sdata *sdata)
{
	if (sdata->type & FT_SEARCH_FILENAME)
	{
		if (sdata->f_query)
		{
			free (sdata->f_qtokens);
			free (sdata->f_etokens);
		}
	}
	else if (sdata->type & FT_SEARCH_MD5)
	{
		free (sdata->m_query);
	}
}

static int cmp_sdata (struct _sdata *sdata, FileShare *file)
{
	if (!file)
		return FALSE;

	return sdata->sfunc (sdata, file);
}

/*****************************************************************************/

static void add_result (struct _sdata *sdata, FileShare *file)
{
	FTShare *share;

	assert (file != NULL);

	if (sdata->matches >= max_results)
		return;

	if (!(share = share_lookup_data (file, "OpenFT")))
	{
		TRACE (("this shouldnt happen"));
		return;
	}

	assert (share->shost != NULL);

	if (!file->p)
		ft_shost_avail (share->shost->host, sdata->avail);

	ft_share_ref (file);

	/* associate these results with the given share host so that if this
	 * node disconnects while this data is still live we can safely decouple
	 * share->shost */
	share->shost->files = list_prepend (share->shost->files, file);

	/* add to direct matches */
	sdata->results = list_prepend (sdata->results, file);
	sdata->matches++;
}

#ifdef USE_LIBDB
static void import_meta (FileShare *file, char *meta, unsigned short len)
{
	while (len != 0 && *meta)
	{
		char  *key;
		char  *val;
		size_t size;

		key  = meta; meta += strlen (key) + 1;
		val  = meta; meta += strlen (val) + 1;
		size = (meta - key);

		if (len < size)
			break;

		len -= size;

		meta_set (file, key, val);
	}
}

static void add_result_db (struct _sdata *sdata, DBT *key, DBT *data)
{
	FileShare  *file;
	FTShareRec *rec = data->data;

	if (sdata->matches >= max_results)
		return;

	file = ft_share_new (sdata->shost, rec->size, rec->md5,
	                     rec->data + rec->mime, rec->data + rec->path);

	if (!file)
		return;

	import_meta (file, rec->data + rec->meta, rec->data_len - rec->meta);

	add_result (sdata, file);
	ft_share_unref (file);
}

/*****************************************************************************/

struct cursor_stream
{
	DBC      *cursor;
	u_int32_t flags;
};

static int cleanup_matches (DBT *data, void *udata)
{
	free (data->data);
	free (data);
	return TRUE;
}

static int cleanup_cursors (struct cursor_stream *s, void *udata)
{
	(s->cursor)->c_close (s->cursor);
	free (s);
	return TRUE;
}

static void token_cleanup (List *matches, List *cursors)
{
	matches =
		list_foreach_remove (matches, (ListForeachFunc) cleanup_matches, NULL);
	list_free (matches);

	cursors =
	    list_foreach_remove (cursors, (ListForeachFunc) cleanup_cursors, NULL);
	list_free (cursors);
}

static DBC *get_cursor (DB *dbp, ft_uint32 token)
{
	DBC *dbcp;
	DBT  key;
	DBT  value;
	int  ret;

	if (dbp->cursor (dbp, NULL, &dbcp, 0))
		return NULL;

	memset (&key, 0, sizeof (key));
	memset (&value, 0, sizeof (value));

	key.data = &token;
	key.size = sizeof (token);

	/* TODO: how do I set the cursor without actually requiring a lookup of
	 * this data? */
	if ((ret = dbcp->c_get (dbcp, &key, &value, DB_SET)) != 0)
	{
		dbcp->c_close (dbcp);
		return NULL;
	}

	return dbcp;
}

static List *gather_cursors (struct _sdata *sdata, ft_uint32 *tokens)
{
	ft_uint32 *t;
	List      *cursors = NULL;
	DBC       *dbcp;

	for (t = tokens; t && *t; t++)
	{
		struct cursor_stream *s;

		/* one of the tokens searched for failed to lookup completely,
		 * abort the search (and return 0 results) */
		if (!(dbcp = get_cursor (sdata->shost->sec, *t)))
		{
			token_cleanup (NULL, cursors);
			return NULL;
		}

		if (!(s = malloc (sizeof (struct cursor_stream))))
			continue;

		s->cursor = dbcp;
		s->flags  = DB_CURRENT;

		cursors = list_prepend (cursors, s);
	}

	return cursors;
}

static void token_add_result (List **results, DBT *data)
{
	DBT *copy;

	if (!(copy = malloc (sizeof (DBT))))
		return;

	memset (copy, 0, sizeof (DBT));

	copy->size = data->size;

	if (!(copy->data = malloc (copy->size)))
	{
		free (copy);
		return;
	}

	memcpy (copy->data, data->data, copy->size);

	*results = list_prepend (*results, copy);
}

static int look_for (struct cursor_stream *s, DBT *cmp_data)
{
	DBT key;
	DBT data;
	int cmp;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	/*
	 * Attempt to set this cursor to a similar position as the start cursor,
	 * while attempting to locate any possible token intersection according
	 * to the md5sum (compare cmp_data vs data).
	 */
	for (; (s->cursor)->c_get (s->cursor, &key, &data, s->flags) == 0;
	     s->flags = DB_NEXT_DUP)
	{
		cmp = memcmp (data.data, cmp_data->data, cmp_data->size);

		/*
		 * We've gone too far, therefore we know for sure this data does
		 * not exist in the current set.
		 */
		if (cmp > 0)
		{
			/*
			 * This data set is beyond the current set we are looking for,
			 * so we will want to start the next search here.
			 */
			s->flags = DB_CURRENT;
			return FALSE;
		}

		/* matched, note that we will not reset flags as this exact position
		 * will be passed by the parent cursor as well */
		if (cmp == 0)
			return TRUE;
	}

	/* this set has exhausted, no more data left...we should really set
	 * some special cursor flag so that we stop searching this stream. */
	return FALSE;
}

static void calc_shortest (struct cursor_stream *s, void **args)
{
	db_recno_t *count = args[0];
	db_recno_t  scnt;
	int         ret;

	/*
	 * Retrieve the count from the database.  Hopefully this operation should
	 * be inexpensive so that we can justify it's usage.
	 */
	if ((ret = ((s->cursor)->c_count (s->cursor, &scnt, 0))))
	{
		TRACE (("DBcursor->c_count: %s", db_strerror (ret)));
		return;
	}

	/*
	 * This cursor's length is shorter than the last known cursor stream, so
	 * we should reset the smallest length and current "located" stream
	 * value on the args data.
	 */
	if (*count == 0 || scnt < *count)
	{
		*count = scnt;
		args[1] = s;
	}
}

static struct cursor_stream *get_start_cursor (List **qt)
{
	struct cursor_stream *s;
	List      *link;
	void      *args[2];
	db_recno_t count = 0;

	args[0] = &count;
	args[1] = NULL;

	/*
	 * Loop through all cursor streams in order to calculate the shortest
	 * cursor (in terms of number of duplicates).  See below (match_qt) for an
	 * explanation of why we do this.  Note that if we only have one
	 * element in this list we can assume it is the shortest and skip the
	 * cursor count retrieval.
	 */
	if (list_next (*qt))
		list_foreach (*qt, (ListForeachFunc)calc_shortest, args);

	/*
	 * If args[1] is non-NULL, we have located an appropriate node, remove from
	 * the cursor list and return the beginning cursor stream.  If args[1]
	 * is NULL, we should simply pop off the 0th element and return it.
	 */
	if (args[1])
		link = list_find (*qt, args[1]);
	else
		link = list_nth (*qt, 0);

	if (!link)
		return NULL;

	/* we need to assign this before we remove the link as it will be
	 * freed by removal */
	s = link->data;
	*qt = list_remove_link (*qt, link);

	return s;
}

static void match_qt (List **results, List **qt)
{
	struct cursor_stream *s;
	List      *ptr;
	DBT        key;
	DBT        data;
	int        lost;
	size_t     matches = 0;

	if (!(*qt))
		return;

	/*
	 * The start cursor is the cursor used in the outer loop shown below.
	 * The number of duplicate entries in this cursor will determine the
	 * minimum number of loops required, and thus this function will attempt
	 * to locate the shortest (lowest count) cursor.
	 */
	if (!(s = get_start_cursor (qt)))
		return;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	/*
	 * Loop through the start cursor forcing all subsequent cursors to keep
	 * themselves aligned with this current position as determined by the
	 * sorting position of the md5sum it represents.  We are effectively
	 * trying to locate the intersection of all cursor sets so that we can
	 * guarantee a search hit.
	 */
	for (; (s->cursor)->c_get (s->cursor, &key, &data, s->flags) == 0;
	     s->flags = DB_NEXT_DUP)
	{
		lost = FALSE;

		/*
		 * Walk along all the other tokens looking for an intersection.  Note
		 * that this code holds the last position of the cursor so that we
		 * never have to begin the search from the beginning.
		 */
		for (ptr = *qt; ptr; ptr = list_next (ptr))
		{
			if (!look_for (ptr->data, &data))
			{
				lost = TRUE;
				break;
			}
		}

		/*
		 * This token matched all in all cursors.  It should be considered
		 * a positive hit.
		 */
		if (!lost)
		{
			token_add_result (results, &data);

			/* make sure we cap the size of the results */
			if (++matches >= max_results)
				break;
		}
	}

	/*
	 * Put the popped cursor back in the list so that we can still use
	 * the same interface for freeing all streams.
	 */
	*qt = list_prepend (*qt, s);
}

static void match_et (List **results, List **et)
{
	if (!(*results) || !(*et))
		return;
}

static List *token_lookup_match (struct _sdata *sdata, List *qt, List *et)
{
	List *results = NULL;

	match_qt (&results, &qt);
	match_et (&results, &et);

	token_cleanup (NULL, qt);
	token_cleanup (NULL, et);

	return results;
}

/* TODO -- implement this using DB_DBT_PARTIAL */
static int token_complete_match (struct _sdata *sdata, DB *dbp,
                                 DBT *key, DBT *data)
{
	FTShareRec *rec = data->data;

	if (sdata->f_realm)
	{
		char *mime = rec->data + rec->mime;

		if (strncmp (sdata->f_realm, mime, strlen (sdata->f_realm)))
			return FALSE;
	}

	return TRUE;
}

static int lookup_ret (DBT *dbt, struct _sdata *sdata)
{
	DB *pri;
	DBT data;
	int ret;

	if (!(pri = ft_shost_pri (sdata->shost)))
		return TRUE;

	if (sdata->matches >= max_results)
		return TRUE;

	memset (&data, 0, sizeof (data));

	if ((ret = pri->get (pri, NULL, dbt, &data, 0)))
		TRACE (("DB->get failed: %s", db_strerror (ret)));
	else
	{
		/* add the result */
		if (token_complete_match (sdata, pri, dbt, &data))
			add_result_db (sdata, dbt, &data);
	}

	cleanup_matches (dbt, NULL);

	return TRUE;
}

static void token_lookup_ret (struct _sdata *sdata, List *matches)
{
	matches =
		list_foreach_remove (matches, (ListForeachFunc) lookup_ret, sdata);
	list_free (matches);
}

static void token_lookup (struct _sdata *sdata)
{
	List *qt_cursors = NULL;
	List *et_cursors = NULL;
	List *cursors = NULL;

	qt_cursors = gather_cursors (sdata, sdata->f_qtokens);
	et_cursors = gather_cursors (sdata, sdata->f_etokens);

	/* find the list of cursors which successfully matched this query */
	if (!(cursors = token_lookup_match (sdata, qt_cursors, et_cursors)))
		return;

	/* handles cursors cleanup */
	token_lookup_ret (sdata, cursors);
}
#endif /* USE_LIBDB */

/*****************************************************************************/

static void search_remote_host (struct _sdata *sdata)
{
#ifdef USE_LIBDB
	DB *dbp = NULL;
	DBT key;
	DBT data;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	if (sdata->type & FT_SEARCH_MD5)
	{
		if (!(dbp = ft_shost_pri (sdata->shost)))
			return;

		key.data = sdata->m_query;
		key.size = 16;

		if (dbp->get (dbp, NULL, &key, &data, 0))
			return;

		add_result_db (sdata, &key, &data);
	}
#if 0
	else if (sdata->type & FT_SEARCH_HOST)
	{
		DBC *dbcp;

		dbp = sdata->shost->pri;

		if (dbp->cursor (dbp, NULL, &dbcp, 0))
			return;

		while (dbcp->c_get (dbcp, &key, &data, DB_NEXT) == 0)
		{
			add_result_db (sdata, &key, &data);

			if (sdata->matches >= max_results)
				break;
		}

		dbcp->c_close (dbcp);
	}
#endif
	else if (sdata->type & FT_SEARCH_FILENAME)
	{
		/* bum bum bummmmmm ... this is the most complex search type by
		 * far */
		token_lookup (sdata);
	}
#endif /* USE_LIBDB */
}

static int key_as_host (Dataset *d, DatasetNode *node,
						struct _sdata *sdata)
{
	FTSHost *shost = node->value;

	if (sdata->matches >= max_results)
		return FALSE;

#if 0
	if (!shost->pri || !shost->sec)
		return TRUE;
#endif

	sdata->shost = shost;
	search_remote_host (sdata);
	sdata->shost = NULL;

	return TRUE;
}

static void search_remote (struct _sdata *sdata)
{
	if (sdata->matches >= max_results)
		return;

#if 0
	if (sdata->type & FT_SEARCH_HOST)
	{
		DatasetNode *node;

		/* skip directly to the appropriate host object */
		node = dataset_lookup_node (ft_shosts (), &sdata->h_query,
									sizeof (sdata->h_query));

		if (node)
			key_as_host (ft_shosts (), node, sdata);
	}
	else
#endif
	{
		/* look at all hosts individually */
		dataset_foreach_ex (ft_shosts (), DATASET_FOREACH (key_as_host),
		                    sdata);
	}
}

static int key_as_md5 (Dataset *d, DatasetNode *node,
					   struct _sdata *sdata)
{
	if (sdata->matches >= max_results)
		return FALSE;

	/* use the primitive/slow matching routines */
	if (sdata->sfunc (sdata, node->value))
		add_result (sdata, node->value);

	return TRUE;
}

static void search_local (struct _sdata *sdata)
{
	if (!(sdata->type & FT_SEARCH_LOCAL) || sdata->matches >= max_results)
		return;

	sdata->avail = upload_availability ();

	sdata->shost = NULL;
	share_foreach (DATASET_FOREACH (key_as_md5), sdata);
}

/*****************************************************************************/

/*
 * and the fun begins...
 */
List *ft_search (size_t *matches, FTSearchType type, char *realm,
				 void *query, void *exclude)
{
	StopWatch *sw;
	struct _sdata sdata;

	if (!query)
		return NULL;

	if (!fill_sdata (&sdata, type, realm, query, exclude))
		return NULL;

	max_results = config_get_int (OPENFT->conf, "search/max_results=1000");

#ifdef DEBUG
	sw = stopwatch_new (FALSE);
	stopwatch_start (sw);
#endif /* DEBUG */

	/* search the files we are currently sharing */
	search_local (&sdata);

	/* search the remote nodes databases
	 * NOTE: this does not mean to search over OpenFT, merely look at
	 * shares that were submitted via OpenFT from our CHILD nodes */
	search_remote (&sdata);

#ifdef DEBUG
	stopwatch_stop (sw);

	if (!(type & FT_SEARCH_MD5))
	{
		TRACE (("elapsed: %i: %.06f", (int) type,
		        stopwatch_elapsed (sw, NULL)));
	}

	stopwatch_free (sw);
#endif /* DEBUG */

	clear_sdata (&sdata);

	if (matches)
		*matches = sdata.matches;

	return sdata.results;
}

int ft_search_cmp (FileShare *file, FTSearchType type,
                   char *realm, void *query, void *exclude)
{
	FTShare *share;
	struct _sdata sdata;
	int ret;

	if (!query || !ft_share_complete (file))
		return FALSE;

	if (!fill_sdata (&sdata, type, realm, query, exclude))
		return FALSE;

	if ((share = share_lookup_data (file, "OpenFT")))
		sdata.shost = share->shost;

	ret = cmp_sdata (&sdata, file);

	clear_sdata (&sdata);

	return ret;
}
