/*
 * $Id: ft_search_exec.c,v 1.66 2004/08/22 01:52:40 hexwab Exp $
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

#include <errno.h>
#include <ctype.h>

#include "ft_openft.h"

#if 0
#include "src/share_cache.h"           /* ILLEGAL! */
#else
void share_foreach (DatasetForeachExFn foreach_fn, void *data);
#endif

#include <libgift/stopwatch.h>

#include "ft_search.h"
#include "ft_search_db.h"
#include "ft_transfer.h"

#include "md5.h"

#include "ft_search_exec.h"
#include "ft_tokenize.h"

/*****************************************************************************/

/*
 * Enable if you wish debugging information regarding the elapsed search time
 * to be displayed.
 */
/* #define SEARCH_TIMING */

/*****************************************************************************/

struct sdata;
typedef BOOL (*FTSearchFunc) (struct sdata *sdata, FileShare *File);

/*
 * Holds search query information as it passes through the set of functions
 * defined here.  This is useful to avoid passing multiple arguments at a time
 * and nothing more.
 */
typedef struct sdata
{
	FTNode           *node;            /* cached node that owns the shares
	                                    * we are currently iterating */
	size_t            nmax;
	size_t            matches;         /* number of successful matches */

	FTSearchResultFn  resultfn;        /* callback when a result is found */
	void             *udata;           /* user data for the above callback */

	ft_search_flags_t type;            /* type of search we are performing */
	FTSearchFunc      sfunc;           /* local matching function for this
	                                    * search type */

	/**
	 * @name FT_SEARCH_FILENAME
	 */
	unsigned long     avail;           /* cached availability */

	char             *f_query;         /* query string */
	char             *f_exclude;       /* exclude string */
	struct tokenized *f_qtok;          /* query tokens list */
	struct tokenized *f_etok;          /* exclude tokens list */
	char             *f_realm;         /* optional realm to filter by */

	/**
	 * @name FT_SEARCH_MD5
	 */
	unsigned char    *m_query;         /* 16-byte md5sum */

	/**
	 * @name FT_SEARCH_HOST
	 *
	 * This is partially deprecated.  It is only used to check incoming
	 * browse results now.
	 */
	in_addr_t         h_query;         /* 4-byte host */
} SearchData;

/*****************************************************************************/
/*
 * COMPARE FUNCTIONS
 *
 * NOTE:
 * These are used primarily on the user node side to verify incoming search
 * results.
 */

static BOOL cmp_filename (SearchData *sdata, FileShare *file)
{
	FTShare   *share;
	uint32_t  *tptr;
	uint32_t  *ptr;
	int        ret = TRUE;
	int        i, j;
	
	if (sdata->f_realm)
	{
		if (strncmp (file->mime, sdata->f_realm, strlen (sdata->f_realm)))
			return FALSE;
	}

	if (!(share = share_get_udata (file, "OpenFT")))
		return FALSE;

	/* check exclusion first */
	for (i = 0, ptr = sdata->f_etok->tokens; i < sdata->f_etok->len; i++, ptr++)
	{
		for (j = 0, tptr = share->tokens->tokens; j < share->tokens->len; j++, tptr++)
		{
			if (*tptr != *ptr)
				continue;

			/* matched exclusion, bail out */
			return FALSE;
		}
	}

	/* check main token list */
	for (i = 0, ptr = sdata->f_qtok->tokens; i < sdata->f_qtok->len; i++, ptr++)
	{
		int matched = FALSE;

		for (j = 0, tptr = share->tokens->tokens; j < share->tokens->len; j++, tptr++)
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

static BOOL cmp_md5 (SearchData *sdata, FileShare *file)
{
	Hash *hash;

	if (!(hash = share_get_hash (file, "MD5")))
		return FALSE;

	return (memcmp (hash->data, sdata->m_query, hash->len) == 0);
}

static BOOL cmp_host (SearchData *sdata, FileShare *file)
{
	/* non-local search */
	if (sdata->node)
		return (sdata->node->ninfo.host == sdata->h_query);

	/*
	 * Local search.
	 *
	 * NOTE:
	 * We don't have the ip address of the local node, therefore we cannot
	 * verify that this was in fact a match.
	 *
	 * TODO:
	 * Search by host should not be done like this...sigh.
	 */
	return FALSE;
}

/*****************************************************************************/

static int fill_sdata (SearchData *sdata, int nmax,
                       FTSearchResultFn resultfn, void *udata,
                       ft_search_flags_t type, const char *realm,
                       void *query, void *exclude)
{
	if (!query)
		return FALSE;

	memset (sdata, 0, sizeof (SearchData));

	sdata->nmax     = (size_t)nmax;    /* maximum results */
	sdata->resultfn = resultfn;        /* cb used when result confirmed */
	sdata->udata    = udata;           /* arbitrary user data for resultfn */
	sdata->type     = type;            /* type of search */

	switch (FT_SEARCH_METHOD(sdata->type))
	{
	 case FT_SEARCH_FILENAME:
		{
			struct tokenized *qtokens;
			struct tokenized *etokens;

			/* hidden searches are deprecated */
			if (sdata->type & FT_SEARCH_HIDDEN)
				return FALSE;

			if (!(qtokens = ft_tokenize_query (query, TOKENIZE_ORDER | TOKENIZE_COUNT)))
				return FALSE;

			etokens = ft_tokenize_query (exclude, 0);

			sdata->sfunc     = (FTSearchFunc)cmp_filename;
			sdata->f_query   = query;
			sdata->f_exclude = exclude;
			sdata->f_qtok    = qtokens;
			sdata->f_etok    = etokens;
			sdata->f_realm   = (char *)realm;
		}
		break;
	 case FT_SEARCH_MD5:
		{
			sdata->sfunc = (FTSearchFunc)cmp_md5;

			if (!(sdata->m_query = md5_bin (query)))
				return FALSE;
		}
		break;
	 case FT_SEARCH_HOST:
		{
			sdata->sfunc = (FTSearchFunc)cmp_host;

			if (!(sdata->h_query = net_ip (query)))
				return FALSE;
		}
		break;
	 default:
		abort ();
		break;
	}

	return (sdata->sfunc ? TRUE : FALSE);
}

static void clear_sdata (SearchData *sdata)
{
	int ret;

	if (sdata->resultfn)
	{
		ret = sdata->resultfn (NULL, sdata->udata);
		assert (ret == TRUE);
	}

	if (FT_SEARCH_METHOD(sdata->type) == FT_SEARCH_FILENAME)
	{
		if (sdata->f_query)
		{
			ft_tokenize_free (sdata->f_qtok);
			ft_tokenize_free (sdata->f_etok);
		}
	}
	else if (FT_SEARCH_METHOD(sdata->type) == FT_SEARCH_MD5)
	{
		free (sdata->m_query);
	}
}

static BOOL cmp_sdata (SearchData *sdata, FileShare *file)
{
	if (!file)
		return FALSE;

	return sdata->sfunc (sdata, file);
}

/*****************************************************************************/

static void add_result (SearchData *sdata, FileShare *file)
{
	FTShare *share;

	assert (file != NULL);

	if (sdata->matches >= sdata->nmax)
		return;

	if (!(share = share_get_udata (file, "OpenFT")))
	{
		FT->DBGFN (FT, "this shouldnt happen");
		return;
	}

	/* share->node will be NULL when this record exists for our local node */
	if (share->node)
		assert (share->node->session != NULL);
	else
		openft->avail = sdata->avail;

	ft_share_ref (file);

	if (sdata->resultfn (file, sdata->udata))
		sdata->matches++;
}

/*****************************************************************************/

static int final_match (FileShare *file, SearchData *sdata)
{
	/* all we really need to require is that the specified realm matches the
	 * result, if supplied */
	if (!sdata->f_realm)
		return TRUE;

	return (strncmp (file->mime, sdata->f_realm, strlen (sdata->f_realm)) == 0);
}

static void search_remote (SearchData *sdata)
{
	Array     *matches = NULL;
	FileShare *match;
	int        hits;
	int        max_hits;

	if (sdata->matches >= sdata->nmax)
		return;

	/* set the maximum allowed matches for this search method */
	max_hits = sdata->nmax - sdata->matches;

	/*
	 * Perform the appropriate search, filling the matches array with dirty
	 * hits.  Further verification will be performed after this block,
	 * however it is clear that this checking should occur in ft_search_db.c,
	 * consider this a TODO :)
	 */
	switch (FT_SEARCH_METHOD(sdata->type))
	{
	 case FT_SEARCH_MD5:
		hits = ft_search_db_md5 (&matches, sdata->m_query, max_hits);
		break;
	 case FT_SEARCH_FILENAME:
		hits = ft_search_db_tokens (&matches, sdata->f_realm,
		                            sdata->f_qtok, sdata->f_etok,
					    max_hits);
		break;
	 default:
		abort ();                      /* shouldnt happen */
		break;
	}

	/*
	 * Iterate all matches simply to go through the regular add_result API
	 * unification.  This is actually much more wasteful than you'd think
	 * considering the add_result API doesn't even need a buffered list to
	 * operate on...sigh.
	 */
	while ((match = array_shift (&matches)))
	{
		/* apply the final clean match on the result to make sure it fits all
		 * the search criteria */
		if (final_match (match, sdata))
			add_result (sdata, match);

		/* if final match fails, ref will be 1 here, and thus the data
		 * cleared...alternatively, if the resultfn decides it does not need
		 * to buffer, it will unref once for itself and the data here will be
		 * cleared */
		ft_share_unref (match);
	}

	array_unset (&matches);
}

static int key_as_md5 (ds_data_t *key, ds_data_t *value,
                       SearchData *sdata)
{
	if (sdata->matches >= sdata->nmax)
		return DS_BREAK;

	/* use the iterative matching routines */
	if (sdata->sfunc (sdata, value->data))
		add_result (sdata, value->data);

	return DS_CONTINUE;
}

static void search_local (SearchData *sdata)
{
	if (!(sdata->type & FT_SEARCH_LOCAL) || sdata->matches >= sdata->nmax)
		return;

	sdata->avail = ft_upload_avail ();

	sdata->node = NULL;
	share_foreach (DS_FOREACH_EX(key_as_md5), sdata);
}

/*****************************************************************************/

/*
 * and the fun begins...
 */
int ft_search (int nmax, FTSearchResultFn resultfn, void *udata,
               ft_search_flags_t type, const char *realm,
               void *query, void *exclude)
{
#ifdef SEARCH_TIMING
	StopWatch *sw;
#endif /* SEARCH_TIMING */
	SearchData sdata;
	int        results;

	if (!query)
		return -1;

	/* ensure that nmax is a sane value */
	if (nmax > FT_CFG_SEARCH_RESULTS || nmax <= 0)
		nmax = FT_CFG_SEARCH_RESULTS;

	if (!fill_sdata (&sdata, nmax, resultfn, udata, type, realm, query, exclude))
		return -1;

#ifdef SEARCH_TIMING
	sw = stopwatch_new (TRUE);
#endif /* SEARCH_TIMING */

	/* search the remote nodes databases
	 * NOTE: this does not mean to search over OpenFT, merely look at
	 * shares that were submitted via OpenFT from our CHILD nodes */
	search_remote (&sdata);

#ifdef SEARCH_TIMING
	if (FT_SEARCH_METHOD(type) == FT_SEARCH_FILENAME)
	{
		FT->DBGFN (FT, "%i(%i): %.06fs elapsed", (int)type, (int)sdata.matches,
		           stopwatch_elapsed (sw, NULL));
	}

	stopwatch_free (sw);
#endif /* SEARCH_TIMING */

	/* destroy any allocated data we had created in fill_sdata */
	results = (int)sdata.matches;
	clear_sdata (&sdata);

	assert (results <= nmax);
	return results;
}

BOOL ft_search_cmp (Share *file, ft_search_flags_t type,
                    const char *realm, void *query, void *exclude)
{
	FTShare *share;
	SearchData sdata;
	BOOL ret;

	if (!query || !ft_share_complete (file))
		return FALSE;

	if (!fill_sdata (&sdata, 1, NULL, NULL, type, realm, query, exclude))
		return FALSE;

	if ((share = share_get_udata (file, "OpenFT")))
		sdata.node = share->node;

	ret = cmp_sdata (&sdata, file);

	clear_sdata (&sdata);

	return ret;
}
