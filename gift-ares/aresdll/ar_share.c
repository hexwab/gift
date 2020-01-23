/*
 * $Id: ar_share.c,v 1.1 2005/12/18 16:43:38 mkern Exp $
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

static as_bool queueing = FALSE;   /* TRUE if ar_share_begin was called and
                                    * we are adding shares to queued_shares */
static List *queued_shares = NULL;

/*****************************************************************************/

/*
 * Begin addition of shares. For efficiency reasons multiple calls to
 * ar_share_add should be sandwiched using ar_share_begin and ar_share_end.
 * Will fail if ar_share_begin has been called before without matching
 * ar_share_end.
 */
as_bool ar_share_begin ()
{
	if (queueing)
	{
		AS_ERR ("ar_share_begin called multiple times");
		return FALSE;
	}

	assert (queued_shares == NULL);
	queueing = TRUE;

	return TRUE;
}

/*
 * End addition of shares. 
 */
as_bool ar_share_end ()
{
	as_bool ret = TRUE;

	if (!queueing)
	{
		AS_ERR ("ar_share_end called without ar_share_begin");
		return FALSE;
	}

	if (queued_shares)
	{
		if (!ar_events_pause ())
			return FALSE;

		ret = as_shareman_add_and_submit (AS->shareman, queued_shares);

		ar_events_resume ();
	}

	queued_shares = list_free (queued_shares);
	queueing = FALSE;

	return ret;
}

/*
 * Add share. If share->hash is NULL the hash will be calculated before the
 * function returns (very inefficient to do all the time). If the file does
 * not fall into one of Ares' realms or it is an incomplete download it will
 * not be shared and FALSE will be returned.
 */
as_bool ar_share_add (const ARShare *share)
{
	ASShare *s;
	ASHash *hash = NULL;
	ASMeta *meta;
	ASRealm realm;
	as_bool ret;

	/* Don't share this file since Ares wouldn't either. */
	if ((realm = as_meta_realm_from_filename (share->path)) == REALM_UNKNOWN)
	{
		return FALSE;
	}

	/* Don't share incomplete files */
	if (strncmp (as_get_filename (share->path),
	             AS_DOWNLOAD_INCOMPLETE_PREFIX,
		         strlen (AS_DOWNLOAD_INCOMPLETE_PREFIX)) == 0)
	{
		return FALSE;
	}


	/* Create internal share object */
	if (share->hash)
	{
		assert (AR_HASH_SIZE == AS_HASH_SIZE);

		if (!(hash = as_hash_create ((as_uint8 *)share->hash, AS_HASH_SIZE)))
		{
			AS_ERR ("Insufficient memory.");
			return FALSE;
		}
	}

	if (!(meta = ar_import_meta ((ARMetaTag *)share->meta)))
	{
		AS_ERR ("Failed to import meta data.");
		as_hash_free (hash);
		return FALSE;
	}

	/* takes ownership of hash and meta objects */
	if (!(s = as_share_create (share->path, hash, meta, share->size, realm)))
	{
		AS_ERR_1 ("Failed to create share object for '%s'", share->path);
		as_hash_free (hash);
		as_meta_free (meta);
		return FALSE;
	}

	/* Add share. */
	if (queueing)
	{
		/* If we are queueing there is no need to interrupt event system. */
		queued_shares = list_prepend (queued_shares, s);
		ret = TRUE;	
	}
	else
	{	
		/* Not queueing, add share directly. */
		List *link;

		if (!ar_events_pause ())
			return FALSE;
		
		link = list_prepend (NULL, s);
		ret = as_shareman_add_and_submit (AS->shareman, link);
		link = list_free (link);

		ar_events_resume ();
	}

	return ret;
}

/*****************************************************************************/

