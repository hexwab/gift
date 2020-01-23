/*
 * $Id: fst_meta.c,v 1.14 2004/11/18 18:16:48 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#include "fst_fasttrack.h"
#include "fst_meta.h"

/*****************************************************************************/

struct
{
	FSTFileTag tag;
	FSTFileTagData data_type;
	char *name;
} 
TagTable[] = 
{
	{ FILE_TAG_YEAR,        FILE_TAG_DATA_INT,    "year"        },
	{ FILE_TAG_FILENAME,    FILE_TAG_DATA_STRING, "filename"    },
    { FILE_TAG_HASH,        FILE_TAG_DATA_RAW,    "hash"        },
	{ FILE_TAG_TITLE,       FILE_TAG_DATA_STRING, "title"       },
	{ FILE_TAG_TIME,        FILE_TAG_DATA_INT,    "duration"    },
    { FILE_TAG_ARTIST,      FILE_TAG_DATA_STRING, "artist"      },
	{ FILE_TAG_ALBUM,       FILE_TAG_DATA_STRING, "album"       },
	{ FILE_TAG_LANGUAGE,    FILE_TAG_DATA_STRING, "language"    },
	{ FILE_TAG_KEYWORDS,    FILE_TAG_DATA_STRING, "keywords"    },
	{ FILE_TAG_RESOLUTION,  FILE_TAG_DATA_INTINT, "resolution"  },
    { FILE_TAG_CATEGORY,    FILE_TAG_DATA_STRING, "category"    },
	{ FILE_TAG_OS,          FILE_TAG_DATA_STRING, "os"          },
	{ FILE_TAG_BITDEPTH,    FILE_TAG_DATA_INT,    "bitdepth"    },
	{ FILE_TAG_TYPE,        FILE_TAG_DATA_STRING, "type"        },
	{ FILE_TAG_QUALITY,     FILE_TAG_DATA_INT,    "bitrate"     },
    { FILE_TAG_VERSION,     FILE_TAG_DATA_STRING, "version"     },
	{ FILE_TAG_COMMENT,     FILE_TAG_DATA_STRING, "comment"     },
	{ FILE_TAG_CODEC,       FILE_TAG_DATA_STRING, "codec"       },
	{ FILE_TAG_RATING,      FILE_TAG_DATA_INT,    "rating"      },
    { FILE_TAG_SIZE,        FILE_TAG_DATA_INT,    "size"        },
/*
    { FILE_TAG_UNKNOWN1,    FILE_TAG_DATA_INT,    "UNKNOWN1"    },
    { FILE_TAG_UNKNOWN2,    FILE_TAG_DATA_INT,    "UNKNOWN2"    },
*/
	{ FILE_TAG_ANY,         FILE_TAG_DATA_ANY,    NULL          }
};


struct
{
	char *mime;
	FSTMediaType type;
}
MediaTable[] =
{
	/* special cases */
	{ "application/pdf",               MEDIA_TYPE_DOCUMENT },
	{ "application/msword",            MEDIA_TYPE_DOCUMENT },
	{ "application/rtf",               MEDIA_TYPE_DOCUMENT },
	{ "application/pdf",               MEDIA_TYPE_DOCUMENT },
	{ "application/postscript",        MEDIA_TYPE_DOCUMENT },
	{ "application/msaccess",          MEDIA_TYPE_DOCUMENT },
	{ "application/vnd.ms-excel",      MEDIA_TYPE_DOCUMENT },
	{ "application/vnd.ms-powerpoint", MEDIA_TYPE_DOCUMENT },
	{ "application/wordperfect5.1",    MEDIA_TYPE_DOCUMENT },
	{ "application/x-latex",           MEDIA_TYPE_DOCUMENT },
	{ "application/x-star-office",     MEDIA_TYPE_DOCUMENT },

	/* fake software category for MS executables */
	{ "application/x-msdos-program",   MEDIA_TYPE_SOFTWARE },

	/* general cases */
	{ "audio/", MEDIA_TYPE_AUDIO },
	{ "video/", MEDIA_TYPE_VIDEO },
	{ "image/", MEDIA_TYPE_IMAGE },
	{ "text/",  MEDIA_TYPE_DOCUMENT },

	{ NULL,  MEDIA_TYPE_UNKNOWN }
};


/*****************************************************************************/

FSTMediaType fst_meta_mediatype_from_mime (const char *mime)
{
	int i;
	char *lo_mime;

	if (!mime)
		return MEDIA_TYPE_UNKNOWN;

	lo_mime = strdup (mime);
	string_lower (lo_mime);

	for (i=0; MediaTable[i].mime; i++)
	{
		if (!strncmp (MediaTable[i].mime, lo_mime, strlen (MediaTable[i].mime)))
		{
			free (lo_mime);
			return MediaTable[i].type;
		}
	}

	free (lo_mime);
	return MEDIA_TYPE_UNKNOWN;
}

/*****************************************************************************/

/* returns fasttrack file tag from name */
FSTFileTag fst_meta_tag_from_name (const char *name)
{
	int i;	

	if (!name)
		return FILE_TAG_ANY;

	for (i=0; TagTable[i].name; i++)
	{
		if (gift_strcasecmp (name, TagTable[i].name) == 0)
			return TagTable[i].tag;
	}

	return FILE_TAG_ANY;
}

/* returns static name string from fasttrack file tag */
char *fst_meta_name_from_tag (FSTFileTag tag)
{
	int i;	

	for (i=0; TagTable[i].name; i++)
	{
		if (TagTable[i].tag == tag)
			return TagTable[i].name;
	}

	return NULL;
}

/*****************************************************************************/

/* returns human readable value string from packet data, caller frees */
char *fst_meta_giftstr_from_packet (FSTFileTag tag, FSTPacket *data)
{
	int i;

	/* handle special cases first */
	switch (tag)
	{
	case FILE_TAG_QUALITY:
		return stringf_dup ("%u", fst_packet_get_dynint (data) * 1000);

	case FILE_TAG_RESOLUTION: {
		unsigned int width = fst_packet_get_dynint (data);
		unsigned int height = fst_packet_get_dynint (data);
		
		return stringf_dup ("%ux%u", width, height);
	}

	case FILE_TAG_RATING:
		switch (fst_packet_get_dynint (data))
		{
		case 0: return strdup ("Very poor");
		case 1: return strdup ("Poor");
		case 2: return strdup ("OK");
		case 3: return strdup ("Good");
		case 4: return strdup ("Excellent");
		}
		return NULL;	

	case FILE_TAG_HASH:
		/* TODO: use FSTHash */
		return fst_utils_base64_encode (data->read_ptr, fst_packet_remaining (data));

	default:
		break;
	}

	/* handle general strings and integers */
	for (i=0; TagTable[i].name; i++)
	{
		if (TagTable[i].tag == tag)
		{
			if (TagTable[i].data_type == FILE_TAG_DATA_STRING)
			{
				return fst_packet_get_str (data, fst_packet_remaining (data));
			}
			else if (TagTable[i].data_type == FILE_TAG_DATA_INT)
			{
				return stringf_dup ("%u", fst_packet_get_dynint (data));
			}

			return NULL;
		}
	}

/*
	FST_HEAVY_DBG_1 ("WARNING: Unknown meta tag: 0x%08X", tag);
*/
	return NULL;
}

/* returns packet for one meta tag, caller frees packet */
FSTPacket *fst_meta_packet_from_giftstr (const char *name, const char *value)
{
	FSTPacket *packet, *data;
	FSTFileTag tag;
	int i,v,w;
/*
	unsigned char *hash;
*/

	if (!name || !value)
		return NULL;

	if ((tag = fst_meta_tag_from_name (name)) == FILE_TAG_ANY)
		return NULL;

	if (! (data = fst_packet_create ()))
		return NULL;

	/* handle special cases first */
	switch (tag)
	{
	case FILE_TAG_QUALITY:
		fst_packet_put_dynint (data, atol (value) / 1000);
		break;

	case FILE_TAG_RESOLUTION:
		sscanf (value, "%dx%d", &v, &w);
		fst_packet_put_dynint (data, v);
		fst_packet_put_dynint (data, w);
		break;

	case FILE_TAG_RATING:
		if      (!gift_strcasecmp (value, "Very poor")) v = 0;
		else if (!gift_strcasecmp (value, "Poor"))      v = 1;
		else if (!gift_strcasecmp (value, "OK"))        v = 2;
		else if (!gift_strcasecmp (value, "Good"))      v = 3;
		else if (!gift_strcasecmp (value, "Excellent")) v = 4;
		else break;

		fst_packet_put_dynint (data, v);
		break;

	case FILE_TAG_HASH:
		/* TODO: use FSTHash. But shouldn't get any hash here anyway */
		assert (0);
/*
		if ((hash = fst_utils_base64_decode (value, &v)))
		{
			if (v == FST_HASH_LEN)
				fst_packet_put_ustr (data, hash, v);
			free (hash);
		}
*/
		break;

	default:
		break;
	}

	/* if not special case handle general strings and integers */
	if (!fst_packet_size (data))
	{
		for (i=0; TagTable[i].name; i++)
		{
			if (gift_strcasecmp (TagTable[i].name, name) == 0)
			{
				if (TagTable[i].data_type == FILE_TAG_DATA_STRING)
				{
					fst_packet_put_ustr (data, (unsigned char*)value, strlen (value));
				}
				else if (TagTable[i].data_type == FILE_TAG_DATA_INT)
				{
					fst_packet_put_dynint (data, atol (value));
				}

				break;
			}
		}
	}
	
	if (!fst_packet_size (data))
	{
		/* unknown meta data */
		fst_packet_free (data);
		return NULL;
	}

	/* create packet */
	if ((packet = fst_packet_create ()))
	{
		fst_packet_put_dynint (packet, tag);
		fst_packet_put_dynint (packet, fst_packet_size (data));
		fst_packet_rewind (data);
		fst_packet_append (packet, data);
	}

	fst_packet_free (data);
	return packet;
}

/* returns value as used in http replies, caller frees */
char *fst_meta_httpstr_from_giftstr (const char *name, const char *value)
{
	FSTFileTag tag;
	int i;

	if (!name || !value)
		return NULL;

	if ((tag = fst_meta_tag_from_name (name)) == FILE_TAG_ANY)
		return NULL;

	/* handle special cases first */
	switch (tag)
	{
	case FILE_TAG_QUALITY:	
		return stringf_dup ("%u", atol (value) / 1000);

	case FILE_TAG_RESOLUTION:
		/* NOTE: value is e.g. "123x567" here
		 * kazaa seems to only send "123" which makes little sense.
		 * we send the full string, a parser will most likely
		 * stop at 'x' and still get it right.
		 */
		return strdup (value);

	case FILE_TAG_RATING:
		if      (!gift_strcasecmp (value, "Very poor")) return strdup ("0");
		else if (!gift_strcasecmp (value, "Poor"))      return strdup ("1");
		else if (!gift_strcasecmp (value, "OK"))        return strdup ("2");
		else if (!gift_strcasecmp (value, "Good"))      return strdup ("3");
		else if (!gift_strcasecmp (value, "Excellent")) return strdup ("4");

		return NULL;

	case FILE_TAG_HASH:
		/* TODO: use FSTHash. But shouldn't get any hash here anyway */
		return strdup (value);

	default:
		break;
	}

	/* if not special case handle general strings and integers */
	for (i=0; TagTable[i].name; i++)
	{
		if (gift_strcasecmp (TagTable[i].name, name) == 0)
		{
			if (TagTable[i].data_type == FILE_TAG_DATA_STRING ||
				TagTable[i].data_type == FILE_TAG_DATA_INT)
			{
				return strdup (value);
			}

			return NULL;
		}
	}
	
	return NULL;
}


/*****************************************************************************/


/* alloc and init tag */
FSTMetaTag *fst_metatag_create (char *name, char *value)
{
	FSTMetaTag *tag = malloc (sizeof (FSTMetaTag));

	tag->name = strdup (name);
	tag->value = strdup (value);

	return tag;
}

/* free tag */
void fst_metatag_free (FSTMetaTag *tag)
{
	if (!tag)
		return;

	free (tag->name);
	free (tag->value);
	free (tag);
}

/* alloc and init tag from protocol data */
FSTMetaTag *fst_metatag_create_from_filetag (FSTFileTag tag, FSTPacket *data)
{
	char *name, *value;
	FSTMetaTag *metatag;

	name = fst_meta_name_from_tag (tag);
	value = fst_meta_giftstr_from_packet (tag, data);

	if (!value || !name)
		return NULL;

	metatag = fst_metatag_create (name, value);

	free (value);

	return metatag;
}

/*****************************************************************************/
