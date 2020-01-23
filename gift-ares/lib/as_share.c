/*
 * $Id: as_share.c,v 1.24 2005/01/08 17:25:41 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/***********************************************************************/

ASShare *as_share_create (char *path, ASHash *hash, ASMeta *meta,
                          size_t size, ASRealm realm)
{
	ASShare *share = malloc (sizeof (ASShare));
	char    *filename;

	assert (path);

	if (!share)
		return NULL;
	
	share->path  = strdup (path);
	filename     = as_get_filename (share->path);
	share->ext   = strrchr (filename, '.');
	share->size  = size;
	share->realm = realm;
	share->fake  = FALSE;
	share->udata = NULL;

	if (hash)
	{
		share->hash = hash;
	}
	else
	{
		AS_HEAVY_DBG_1 ("No hash specified for '%s', hashing synchronously.",
		                share->path);

		if (!(share->hash = as_hash_file (share->path)))
		{
			AS_ERR_1 ("Couln't hash file '%s'", share->path);
			free (share->path);
			free (share);
			return NULL;
		}

		AS_HEAVY_DBG_1 ("hash: %s", as_hash_str (share->hash));
	}

	if (!meta)
		meta = as_meta_create ();

	as_meta_add_tag (meta, "filename", filename);

	share->meta = meta;

	return share;
}

/* Create copy of share object. */
ASShare *as_share_copy (ASShare *share)
{
	ASShare *new_share;

	if (!(new_share = malloc (sizeof (ASShare))))
		return NULL;
	
	new_share->path  = strdup (share->path);
	new_share->ext   = strrchr (as_get_filename (new_share->path), '.');
	new_share->size  = share->size;
	new_share->realm = share->realm;
	new_share->fake  = share->fake;
	new_share->udata = share->udata;

	new_share->hash = as_hash_copy (share->hash);
	new_share->meta = as_meta_copy (share->meta);

	return new_share;
}


void as_share_free (ASShare *share)
{
	if (!share)
		return;
	
	free (share->path);
	as_meta_free (share->meta);
	as_hash_free (share->hash);

	free (share);
}

/***********************************************************************/

static as_bool share_tokenize_tag (ASMetaTag *tag, ASPacket *p)
{
	const ASTagMapping1 *map1;
	const ASTagMapping2 *map2;

	/* We need to try both tag types, see as_meta.c for details. */
	if ((map1 = as_meta_mapping1_from_name (tag->name)))
	{
		if (!map1->tokenize)
			return FALSE;
		return !!as_tokenize (p, tag->value, map1->type);
	}
	
	if ((map2 = as_meta_mapping2_from_name (tag->name)))
	{
		if (!map2->tokenize)
			return FALSE;
		return !!as_tokenize (p, tag->value, map2->type);
	}

	return FALSE;
}

static ASPacket *share_add_tokens (ASMeta *meta)
{
	ASPacket *p = as_packet_create ();

	if (!p)
		return NULL;

	as_meta_foreach_tag (meta, (ASMetaForeachFunc)share_tokenize_tag, p);

	return p;
}

static as_bool add_realm_tag (ASPacket *p, ASMeta *meta, ASRealm realm)
{
	as_packet_put_8 (p, TAG_XXX);

	switch (realm)
	{
	case REALM_AUDIO:
	{
		int bitrate, duration;
		bitrate  = as_meta_get_int (meta, "bitrate");
		duration = as_meta_get_int (meta, "duration");
		as_packet_put_le16 (p, (as_uint16) bitrate);
		as_packet_put_le32 (p, (as_uint32) duration);
		break;
	}
	case REALM_IMAGE:
	{
		int width, height;
		width  = as_meta_get_int (meta, "width");
		height = as_meta_get_int (meta, "height");
		as_packet_put_le16 (p, (as_uint16) width);
		as_packet_put_le16 (p, (as_uint16) height);
		as_packet_put_8 (p, 2); /* unknown */
		as_packet_put_le32 (p, 24); /* depth? */
		break;
	}
	case REALM_VIDEO:
	{
		int width, height, duration;
		width    = as_meta_get_int (meta, "width");
		height   = as_meta_get_int (meta, "height");
		duration = as_meta_get_int (meta, "duration");
		as_packet_put_le16 (p, (as_uint16) width);
		as_packet_put_le16 (p, (as_uint16) height);
		as_packet_put_le32 (p, (as_uint32) duration);
		break;
	}
	case REALM_ARCHIVE:
	case REALM_DOCUMENT:
	case REALM_SOFTWARE:
		/* nothing */
		break;
	default:
		assert (0);
		return FALSE;
	}

	return TRUE;
}

static void add_meta_tags1 (ASPacket *p, ASShare *share)
{
	ASMeta *meta = share->meta;
	const ASTagMapping1 *map;
	const char *value;

	/* Add all type 1 tags. */

	if ((map = as_meta_mapping1_from_type (TAG_TITLE)) &&
	    (value = as_meta_get_tag (meta, map->name)))
	{
		as_packet_put_8 (p, (as_uint8) map->type);
		as_packet_put_strnul (p, value);
	}

	if ((map = as_meta_mapping1_from_type (TAG_ARTIST)) &&
	    (value = as_meta_get_tag (meta, map->name)))
	{
		as_packet_put_8 (p, (as_uint8) map->type);
		as_packet_put_strnul (p, value);
	}

	add_realm_tag (p, meta, share->realm);
}

static void add_meta_tags2 (ASPacket *p, ASShare *share)
{
	ASMeta *meta = share->meta;
	const ASTagMapping2 *map;
	int i;

	/* Add all type 2 tags in order (not sure if order is necessary). */
	for (i = 0; i <= 16; i++)
	{
		const char *value;

		if ((map = as_meta_mapping2_from_type (i)) &&
		    (value = as_meta_get_tag (meta, map->name)))
		{
			as_packet_put_8 (p, (as_uint8) map->type);
			as_packet_put_strnul (p, value);
		}
	}
}

ASPacket *as_share_packet (ASShare *share)
{
	ASPacket *p = as_packet_create (), *tokens;

	if (!p)
		return NULL;

	tokens = share_add_tokens (share->meta);

	if (!tokens)
	{
		as_packet_free (p);
		return NULL;
	}
	
	if (tokens->used == 0)
	{
		if (!share->fake)
		{
			as_meta_set_fake (share->meta);
			share->fake = TRUE;
			as_packet_free (tokens);
			tokens = share_add_tokens (share->meta);
			AS_HEAVY_DBG_1 ("faked metadata for '%s'", share->path);
		}

		if (tokens->used == 0)
		{
			as_packet_free (p);
			as_packet_free (tokens);
			return NULL;
		}
	}

	as_packet_put_le16 (p, (as_uint16) tokens->used);
	as_packet_append (p, tokens);
	as_packet_free (tokens);
	
	as_packet_put_le32 (p, as_meta_get_int (share->meta, "bitrate"));
	as_packet_put_le32 (p, as_meta_get_int (share->meta, "frequency"));
	as_packet_put_le32 (p, as_meta_get_int (share->meta, "duration"));
	as_packet_put_8 (p, (as_uint8) share->realm); /* realm */
	as_packet_put_le32 (p, share->size); /* filesize */
	as_packet_put_hash (p, share->hash);
	as_packet_put_strnul (p, share->ext ? share->ext : "");

	/* Add fixed position block. See as_meta.c. */
	add_meta_tags1 (p, share);

	/* Add random position block. See as_meta.c. */
	add_meta_tags2 (p, share);

	return p;
}

/***********************************************************************/

#if 0
int main (int argc, char *argv[])
{
	printf("%s\n", as_get_filename (argv[1]));

	return 0;
}
#endif
