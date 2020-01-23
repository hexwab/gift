/*
 * $Id: ar_search.c,v 1.2 2005/12/18 17:34:20 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "aresdll.h"
#include "as_ares.h"
#include "ar_threading.h"
#include "ar_callback.h"
#include "ar_misc.h"

/*****************************************************************************/

static void result_callback (ASSearch *search, ASResult *result,
                             as_bool duplicate)
{
	ARSearchResult r;

	/* if the search terminated just tell the user app */
	if (!result)
	{
		ar_raise_callback (AR_CB_RESULT, (ARSearchHandle)search, NULL);
		return;
	}

	/* use strings from our internal result object */
	r.duplicate = duplicate;
	r.filename = result->filename;
	r.filesize = result->filesize;
	r.filehash = result->hash->data;

	switch (result->realm)
	{
	case REALM_AUDIO:    r.realm = AR_REALM_AUDIO;    break;
	case REALM_VIDEO:    r.realm = AR_REALM_VIDEO;    break;
	case REALM_DOCUMENT: r.realm = AR_REALM_DOCUMENT; break;
	case REALM_SOFTWARE: r.realm = AR_REALM_SOFTWARE; break;
	case REALM_IMAGE:    r.realm = AR_REALM_IMAGE;    break;
	case REALM_ARCHIVE:  r.realm = AR_REALM_ARCHIVE;  break;
	/* shouldn't happen, but remote controlled */
	default:             r.realm = AR_REALM_ANY;      break;
	}

	ar_export_meta (r.meta, result->meta);

	/* send to user app */
	ar_raise_callback (AR_CB_RESULT, (ARSearchHandle)search, &r);
}

/*****************************************************************************/

/*
 * Start a new search. Returns opaque handle to search or AR_INVALID_HANDLE
 * on failure.
 */
ARSearchHandle ar_search_start (const char *query, ARRealm realm)
{
	ASSearch *search;
	ASRealm r;

	if (!ar_events_pause ())
		return AR_INVALID_HANDLE;

	switch (realm)
	{
	case AR_REALM_ANY:      r = SEARCH_ANY;      break;
	case AR_REALM_AUDIO:    r = SEARCH_AUDIO;    break;
	case AR_REALM_VIDEO:    r = SEARCH_VIDEO;    break;
	case AR_REALM_DOCUMENT: r = SEARCH_DOCUMENT; break;
	case AR_REALM_SOFTWARE: r = SEARCH_SOFTWARE; break;
	case AR_REALM_IMAGE:    r = SEARCH_IMAGE;    break;
	case AR_REALM_ARCHIVE:
		AS_WARN ("Search in realm AR_REALM_ARCHIVE not supported. "
		         "Searching in all realms instead.");
		r = SEARCH_ANY;
		break;
	default: abort ();
	}

	if (!(search = as_searchman_search (AS->searchman, result_callback, query,
	                                    r)))
	{
		ar_events_resume ();
		return AR_INVALID_HANDLE;
	}

	ar_events_resume ();
	return (ARSearchHandle)search;
}

/*
 * Remove search. You cannot start any downloads using the results of a
 * download after removing it.
 */
as_bool ar_search_remove (ARSearchHandle search)
{
	as_bool ret;

	if (!ar_events_pause ())
		return FALSE;

	ret = as_searchman_remove (AS->searchman, (ASSearch *)search);

	ar_events_resume ();
	return ret;
}

/*****************************************************************************/
