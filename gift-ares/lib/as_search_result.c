/*
 * $Id: as_search_result.c,v 1.11 2005/01/05 00:42:32 hex Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static as_bool result_parse (ASResult *r, ASPacket *packet);

/*****************************************************************************/

/* create empty search result */
ASResult *as_result_create ()
{
	ASResult *result;

	if (!(result = malloc (sizeof (ASResult))))
		return NULL;

	result->search_id = INVALID_SEARCH_ID;
	
	if (!(result->source = as_source_create ()))
	{
		free (result);
		return NULL;
	}

	result->meta     = NULL;
	result->realm    = REALM_UNKNOWN;
	result->hash     = NULL;
	result->filesize = 0;
	result->filename = NULL;
	result->fileext  = NULL;
	result->unknown  = 0;
	memset (result->unk, 0, sizeof (result->unk));

	return result;
}


/* create search result from packet */
ASResult *as_result_parse (ASPacket *packet)
{
	ASResult *result;

	if (!(result = as_result_create ()))
		return NULL;

	if (!result_parse (result, packet))
	{
		as_result_free (result);
		return NULL;
	}

	return result;
}

/* free search result */
void as_result_free (ASResult *result)
{
	if (!result)
		return;

	as_source_free (result->source);
	as_meta_free (result->meta);
	as_hash_free (result->hash);

	free (result->filename);
	free (result->fileext);

	free (result);
}

/*****************************************************************************/

static void parse_username (ASResult *r, char *username)
{
	char *netname;

	r->source->username = username;

	if ((netname = strchr (username, '@')))
	{
		*netname = 0;
		r->source->netname = gift_strdup (netname + 1);
	}
}

static void munge_filename (char *filename)
{
	char *ptr;

	if (filename)
	{
		for (ptr = filename; *ptr; ptr++)
		{
			switch (*ptr)
			{
			case '/':
#ifdef WIN32
			case '\\':
			case ':':
#endif
				*ptr = '_';
			}
		}
	}
}

static as_bool result_parse (ASResult *r, ASPacket *packet)
{
	int reply_type;

	/* get data from packet */
	reply_type = as_packet_get_8 (packet);

	switch (reply_type)
	{
	case 0: /* token search result */
		r->search_id = as_packet_get_le16 (packet);
		
		/* supernode IP/port */
		r->source->shost = as_packet_get_ip (packet);
		r->source->sport = as_packet_get_le16 (packet);

		/* user's IP/port */
		r->source->host = as_packet_get_ip (packet);
		r->source->port = as_packet_get_le16 (packet);

		/* bandwidth? */
		r->unknown = as_packet_get_8 (packet);

		/* username */
		parse_username (r, as_packet_get_strnul (packet));

		/* unknown, may be split differently */
		r->unk[0] = as_packet_get_8 (packet);
		r->unk[1] = as_packet_get_8 (packet);
		r->unk[2] = as_packet_get_8 (packet);
		r->unk[3] = as_packet_get_8 (packet);
		r->unk[4] = as_packet_get_8 (packet);

		r->realm = (ASRealm) as_packet_get_8 (packet);
		r->filesize = as_packet_get_le32 (packet);
		r->hash = as_packet_get_hash (packet);
		r->fileext = as_packet_get_strnul (packet);

		/* parse meta data */
		r->meta = as_meta_parse_result (packet, r->realm);

		/* get filename from meta data as special case */
		if ((r->filename = (char *)as_meta_get_tag (r->meta, "filename")))
		{
			r->filename = strdup (r->filename);
			as_meta_remove_tag (r->meta, "filename");
		}
		else
		{
			/* attempt to reconstruct from other tags */
			String *filename = string_new (NULL, 0, 0, TRUE);

			const unsigned char *artist = as_meta_get_tag (r->meta, "artist");
			const unsigned char *title  = as_meta_get_tag (r->meta, "title");
			const unsigned char *album  = as_meta_get_tag (r->meta, "album");
			
			if (artist)
				string_appendf (filename, "%s - ", artist);
			if (album)
				string_appendf (filename, "%s - ", album);
			if (title)
				string_append (filename, title);
			if (r->fileext)
				string_append (filename, r->fileext);

			r->filename = string_free_keep (filename);
		}

		munge_filename (r->filename);

		break;
		
	case 1: /* hash search result */
		/* supernode IP/port */
		r->source->shost = as_packet_get_ip (packet);
		r->source->sport = as_packet_get_le16 (packet);

		/* user's IP/port */
		r->source->host = as_packet_get_ip (packet);
		r->source->port = as_packet_get_le16 (packet);

		/* bandwidth? */
		r->unknown = as_packet_get_8 (packet);

		parse_username (r, as_packet_get_strnul (packet));
		r->hash = as_packet_get_hash (packet);
		r->source->inside_ip = as_packet_get_ip (packet);
		break;

	default:
		AS_WARN_1 ("Unknown search result type %d", reply_type);
		return FALSE;
	}

	/* no hash is bad */
	if (!r->hash)
		return FALSE;
	
	return TRUE;
}

/*****************************************************************************/

#if 0
int main (void)
{
	ASPacket *p = as_packet_slurp();
	ASResult *r = as_result_parse (p);

	as_packet_free (p);
	as_result_free (r);

	return 0;
}
#endif
