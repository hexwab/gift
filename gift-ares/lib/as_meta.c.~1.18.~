/*
 * $Id: as_meta.c,v 1.18 2005/02/28 14:19:15 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* These are only string types that have a type to themselves;
 * realm-specific tags are handled separately. */

static const ASTagMapping1 tag_types_1[] = {
	{ "title",     TAG_TITLE,      TRUE  },
	{ "artist",    TAG_ARTIST,     TRUE  },
	{ "album",     TAG_ALBUM_1,    FALSE }
};

static const ASTagMapping2 tag_types_2[] = {
	{ "category",  TAG_CATEGORY,   TRUE  },
	{ "album",     TAG_ALBUM,      TRUE  },
	{ "comments",  TAG_COMMENTS,   FALSE },
	{ "language",  TAG_LANGUAGE,   FALSE },
	{ "unknown_2", TAG_UNKNOWN_2,  FALSE },
	{ "year",      TAG_YEAR,       FALSE },
	{ "codec",     TAG_CODEC,      FALSE },
	{ "keywords",  TAG_KEYWORDS,   TRUE  },
	{ "filename",  TAG_FILENAME,   FALSE } /* yes, really FALSE :( */
};

#define NUM_TYPES_1 (sizeof(tag_types_1) / sizeof(ASTagMapping1))
#define NUM_TYPES_2 (sizeof(tag_types_2) / sizeof(ASTagMapping2))

/*****************************************************************************/

static const struct file_realms_t
{
	ASRealm realm;
	const char *extensions;
}
file_realms[] = 
{
	{ REALM_AUDIO,    ".mp3.vqf.wma.wav.voc.mod.ra.ram.mid.au.ogg.mp2.mpc"
	                  ".flac.shn.ape" },

	{ REALM_VIDEO,    ".avi.mpeg.asf.mov.fli.flc.lsf.wm.qt.viv.vivo.mpg.mpe"
	                  ".mpa.rm.wmv.divx.m1v.mkv.ogm" },

	{ REALM_IMAGE,    ".gif.jpg.jpeg.bmp.psd.psp.tga.tif.tiff.png" },

	{ REALM_DOCUMENT, ".doc.rtf.pdf.ppt.wri.txt.hlp.lit.book.pps.ps" },

	{ REALM_SOFTWARE, ".exe.msi.vbs.pif.bat.com.scr" },

	{ REALM_ARCHIVE,  ".zip.tar.gz.rar.cab.sit.msi.ace.hqx.iso.nrg.rmp.rv"
	                  ".cif.img.rip.cue.bin.fla.swf.dwg.dxf.wsz.nes.md5" }
};

#define NUM_REALMS (sizeof(file_realms) / sizeof(struct file_realms_t))

/*****************************************************************************/

static as_bool meta_parse_result (ASMeta *meta, ASPacket *p, ASRealm realm);

/*****************************************************************************/

/* create meta data object */
ASMeta *as_meta_create ()
{
	ASMeta *meta;
	
	if (!(meta = malloc (sizeof (ASMeta))))
		return NULL;

	meta->tags = NULL;

	return meta;
}

static int tag_copy_itr (ASMetaTag *tag, ASMeta *new_meta)
{
	ASMetaTag *new_tag;

	if ((new_tag = malloc (sizeof (ASMetaTag))))
	{
		new_tag->name = gift_strdup (tag->name);
		new_tag->value = gift_strdup (tag->value);

		/* insert into list */
		new_meta->tags = list_prepend (new_meta->tags, new_tag);
	}	

	return TRUE;
}

/* create copy of meta data object */
ASMeta *as_meta_copy (ASMeta *meta)
{
	ASMeta *new_meta;

	if (!(new_meta = as_meta_create ()))
		return NULL;

	/* copy tags */
	list_foreach (meta->tags, (ListForeachFunc)tag_copy_itr, new_meta);

	return new_meta;
}

/* create meta data object from search result packet */
ASMeta *as_meta_parse_result (ASPacket *packet, ASRealm realm)
{
	ASMeta *meta;
	
	if (!(meta = as_meta_create ()))
		return NULL;

	meta_parse_result (meta, packet, realm);

	return meta;
}

static int tag_free_itr (ASMetaTag *tag, void *data)
{
	free (tag->name);
	free (tag->value);
	free (tag);

	return TRUE;
}

/* free meta data object */
void as_meta_free (ASMeta *meta)
{
	if (!meta)
		return;

	/* free tags */
	list_foreach_remove (meta->tags, (ListForeachFunc)tag_free_itr, NULL);

	free (meta);
}

/*****************************************************************************/

List *meta_find_tag (ASMeta *meta, const char *name)
{
	List *link;

	for (link = meta->tags; link; link = link->next)
		if (gift_strcasecmp (((ASMetaTag *)link->data)->name, name) == 0)
			return link;

	return NULL;
}

/* add gift style meta tag or update if already present */
as_bool as_meta_add_tag (ASMeta *meta, const char *name, const char *value)
{
	List *link;
	ASMetaTag *tag;

	assert (name);
	assert (value);

	if ((link = meta_find_tag (meta, name)))
	{
		tag = (ASMetaTag *) link->data;
		free (tag->value);
	}
	else
	{
		if (!(tag = malloc (sizeof (ASMetaTag))))
			return FALSE;

		/* insert into list */
		meta->tags = list_prepend (meta->tags, tag);

		tag->name = gift_strdup (name);
	}	

	tag->value = gift_strdup (value);

	return TRUE;
}

/* get gift style meta tag data */
const char *as_meta_get_tag (ASMeta *meta, const char *name)
{
	List *link;

	if (!(link = meta_find_tag (meta, name)))
		return NULL;

	return ((ASMetaTag *) link->data)->value;
}

int as_meta_get_int (ASMeta *meta, const char *name)
{
	const char *value = as_meta_get_tag (meta, name);
	
	if (!value)
		return 0;

	return atoi (value);
}

/* remove gift style meta tag */
as_bool as_meta_remove_tag (ASMeta *meta, const char *name)
{
	List *link;

	if (!(link = meta_find_tag (meta, name)))
		return FALSE;

	tag_free_itr (link->data, NULL);
	
	meta->tags = list_remove_link (meta->tags, link);

	return TRUE;
}

/* call func for each tag */
int as_meta_foreach_tag (ASMeta *meta, ASMetaForeachFunc func, void *udata)
{
	List *link;
	int count = 0;
	
	for (link = meta->tags; link; link = link->next)
		if (func (link->data, udata))
			count++;
	
	return count;
}

/*****************************************************************************/

/* return ASTagMapping1 from gift name */
const ASTagMapping1 *as_meta_mapping1_from_name (const char *name)
{
	int i;
	
	for (i=0; i < NUM_TYPES_1; i++)
		if (!gift_strcasecmp (tag_types_1[i].name, name))
			return &tag_types_1[i];

	return NULL;
}

/* return ASTagMapping1 from ASTagType1 */
const ASTagMapping1 *as_meta_mapping1_from_type (ASTagType1 type)
{
	int i;
	
	for (i=0; i < NUM_TYPES_1; i++)
		if (tag_types_1[i].type == type)
			return &tag_types_1[i];

	return NULL;
}

/* return ASTagMapping2 from gift name */
const ASTagMapping2 *as_meta_mapping2_from_name (const char *name)
{
	int i;
	
	for (i=0; i < NUM_TYPES_2; i++)
		if (!gift_strcasecmp (tag_types_2[i].name, name))
			return &tag_types_2[i];

	return NULL;
}

/* return ASTagMapping2 from ASTagType2 */
const ASTagMapping2 *as_meta_mapping2_from_type (ASTagType2 type)
{
	int i;
	
	for (i=0; i < NUM_TYPES_2; i++)
		if (tag_types_2[i].type == type)
			return &tag_types_2[i];

	return NULL;
}

/*****************************************************************************/

static void meta_add_string (ASMeta *meta, ASPacket *packet, const char *name)
{
	char *value;

	if ((value = as_packet_get_strnul (packet)))
	{
		as_meta_add_tag (meta, name, value);
		free (value);
	}
}

static as_bool meta_parse_result (ASMeta *meta, ASPacket *p, ASRealm realm)
{
	int meta_type;

	/* It seems that some of the tag types we define occur multiple times in
	 * the meta data with completely different data. For example sometimes the
	 * 0x04 type is indeed TAG_XXX and other times it's a language string.
	 * Obviously there must be a way to tell which is which. From looking at
	 * several packet dumps I think that there is first a small block in which
	 * fields are optional but fixed in order. Then follows a second random
	 * order block where types have a different meaning. Presumably the second
	 * block was added at a later time but why the same type values were used
	 * I don't know.
	 */

	/* Fixed position fields (ASTagType1). */
	meta_type = as_packet_get_8 (p);

	if (meta_type == TAG_TITLE)
	{
		meta_add_string (meta, p, as_meta_mapping1_from_type (TAG_TITLE)->name);
		meta_type = as_packet_get_8 (p);
	}

	if (meta_type == TAG_ARTIST)
	{
		meta_add_string (meta, p, as_meta_mapping1_from_type (TAG_ARTIST)->name);
		meta_type = as_packet_get_8 (p);
	}

	if (meta_type == TAG_ALBUM_1)
	{
		meta_add_string (meta, p, as_meta_mapping1_from_type (TAG_ALBUM_1)->name);
		meta_type = as_packet_get_8 (p);
	}

	if (meta_type == TAG_XXX)
	{
		char buf[32];
		int i;

		/* handle the realm-specific/non-string stuff */
		switch (realm)
		{
		case REALM_ARCHIVE:
			/* nothing */
			break;

		case REALM_AUDIO:
			/* bitrate */
			i = as_packet_get_le16 (p);
			sprintf (buf, "%u", i);
			as_meta_add_tag (meta, "bitrate", buf);
				
			/* duration */
			i = as_packet_get_le32 (p);
			sprintf (buf, "%u", i);
			as_meta_add_tag (meta, "duration", buf);
			break;
				
		case REALM_SOFTWARE:
		{
			as_uint8 c = as_packet_get_8 (p);
			if (c != 2 && c != 6)
			{
				AS_DBG_2 ("REALM_SOFTWARE: c=%d, offset %x",
				          c, p->read_ptr - p->data);
#ifdef DEBUG
				as_packet_dump (p);
#endif
			}
				
			/* version */
			free (as_packet_get_strnul (p));
			break;
		}

		case REALM_VIDEO:
			/* width/height */
			i = as_packet_get_le16 (p);
			sprintf (buf, "%u", i);
			as_meta_add_tag (meta, "width", buf);
			i = as_packet_get_le16 (p);
			sprintf (buf, "%u", i);
			as_meta_add_tag (meta, "height", buf);
				
			/* duration? */
			i = as_packet_get_le32 (p);
			sprintf (buf, "%u", i);
			as_meta_add_tag (meta, "duration", buf);
			break;
				
		case REALM_IMAGE:
			/* width/height */
			i = as_packet_get_le16 (p);
			sprintf (buf, "%u", i);
			as_meta_add_tag (meta, "width", buf);
			i = as_packet_get_le16 (p);
			sprintf (buf, "%u", i);
			as_meta_add_tag (meta, "height", buf);
				
			/* unknown (depth?) */
			i = as_packet_get_le32 (p);
			sprintf (buf, "%u", i);
			as_meta_add_tag (meta, "bitdepth?", buf);
			break;

		case REALM_DOCUMENT:
			/* nothing */
			break;

		default:
			/* Because size is implicitly encoded, we have no choice but to
			 * bail when an unknown tag type is encountered.
			 */
			AS_DBG_2 ("Unknown realm %d, offset %x", realm,
			          p->read_ptr - p->data);
#ifdef DEBUG
			as_packet_dump (p);
#endif
			return FALSE;
		}

		meta_type = as_packet_get_8 (p);
	}


	/* Random position fields (ASTagType2). Some packets seem to be padded
	 * with zeros and other stuff at the end. If we reach that abort parsing.
	 */
	while (as_packet_remaining (p) > 2 && meta_type != 0x00)
	{
		const ASTagMapping2 *map;

		/* turn everything into gift style meta tags */
		if ((map = as_meta_mapping2_from_type (meta_type)))
		{
			meta_add_string (meta, p, map->name);
		}
		else
		{
			AS_DBG_2 ("Unknown tag type %d, offset %x", meta_type,
			          p->read_ptr - p->data);
#ifdef HEAVY_DEBUG
			as_packet_dump (p);
#endif
			return FALSE;
		}

		meta_type = as_packet_get_8 (p);
	}


	return TRUE;
}

/*****************************************************************************/

/* Returns realm based on file extension exactly as Ares does. */
ASRealm as_meta_realm_from_filename (const char *path)
{
	char *ext, *p;
	int i, ext_len;

	if (!path)
		return REALM_UNKNOWN;

	if (!(ext = strrchr (path, '.')))
		return REALM_UNKNOWN;

	ext_len = strlen (ext);

	for (i = 0; i < NUM_REALMS; i++)
	{
		if ((p = strstr (file_realms[i].extensions, ext)) && 
		    (p[ext_len] == '.' || p[ext_len] == '\0'))
		{
			return file_realms[i].realm;
		}	
	}
	
	return REALM_UNKNOWN;
}

/*****************************************************************************/

/* Ares only tokenizes (and thus searches) metadata, not filenames. This
 * brain-dead decision is responsible for the following ugly hack.
 */
as_bool as_meta_set_fake (ASMeta *meta)
{
	char *filename, *ext;

	filename = gift_strdup (as_meta_get_tag (meta, "filename"));
	assert (filename);

	if ((ext = strrchr (filename, '.')))
		*ext = '\0';

	/* Ares painstakingly splits the filename up into
	 * hyphen-separated parts, but I think this code is
	 * sufficiently ugly and broken as it is, so we don't
	 * bother. */
	as_meta_add_tag (meta, "title", filename);
	free (filename);

	/* Don't submit filename with fake shares - this may have been
	 * intended as an optimization, but is useful for
	 * distinguishing fake results from real ones, so we emulate
	 * this behaviour. */
	as_meta_remove_tag (meta, "filename");

	return TRUE;
}

/*****************************************************************************/
