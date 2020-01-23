/*
 * Copyright (C) 2003 Markus Kern (mkern@users.sourceforge.net)
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

// alloc and init tag
FSTMetaTag *fst_metatag_create (char *name, char *value)
{
	FSTMetaTag *tag = malloc (sizeof(FSTMetaTag));

	tag->name = strdup (name);
	tag->value = strdup (value);

	return tag;
}

// free tag
void fst_metatag_free (FSTMetaTag *tag)
{
	if(!tag)
		return;

	free (tag->name);
	free (tag->value);
	free (tag);
}

// alloc and init tag from protocol data
FSTMetaTag *fst_metatag_create_from_filetag (FSTFileTag filetag, FSTPacket *data)
{
	FSTMetaTag *tag;
	char buf[64];
	char *name = NULL, *value = NULL;

	switch(filetag)
	{
	case FILE_TAG_YEAR:
		name = "year";
		sprintf (buf, "%d", fst_packet_get_dynint (data));
		value = strdup(buf);
		break;
	case FILE_TAG_QUALITY:
		name = "quality";
		sprintf (buf, "%d kbps", fst_packet_get_dynint (data));
		value = strdup(buf);
		break;
	case FILE_TAG_RESOLUTION:
		name = "resolution";
		sprintf (buf, "%dx%d", fst_packet_get_dynint (data), fst_packet_get_dynint (data));
		value = strdup(buf);
		break;
	case FILE_TAG_BITDEPTH:
		name = "bitdepth";
		sprintf (buf, "%d", fst_packet_get_dynint (data));
		value = strdup(buf);
		break;
	case FILE_TAG_TIME:
		name = "length";
		sprintf (buf, "%d sec", fst_packet_get_dynint (data));
		value = strdup(buf);
		break;

	case FILE_TAG_FILENAME:
		name = "filename";
		value = fst_packet_get_str (data, fst_packet_remaining (data));
		break;
	case FILE_TAG_TITLE:
		name = "title";
		value = fst_packet_get_str (data, fst_packet_remaining (data));
		break;
	case FILE_TAG_ARTIST:
		name = "artist";
		value = fst_packet_get_str (data, fst_packet_remaining (data));
		break;
	case FILE_TAG_ALBUM:
		name = "album";
		value = fst_packet_get_str (data, fst_packet_remaining (data));
		break;
	case FILE_TAG_COMMENT:
		name = "comment";
		value = fst_packet_get_str (data, fst_packet_remaining (data));
		break;
	case FILE_TAG_LANGUAGE:
		name = "language";
		value = fst_packet_get_str (data, fst_packet_remaining (data));
		break;
	case FILE_TAG_KEYWORDS:
		name = "keywords";
		value = fst_packet_get_str (data, fst_packet_remaining (data));
		break;
	case FILE_TAG_GENRE:
		name = "genre";
		value = fst_packet_get_str (data, fst_packet_remaining (data));
		break;
	case FILE_TAG_VERSION:
		name = "version";
		value = fst_packet_get_str (data, fst_packet_remaining (data));
		break;
	case FILE_TAG_TYPE:
		name = "type";
		value = fst_packet_get_str (data, fst_packet_remaining (data));
		break;
	case FILE_TAG_CODEC:
		name = "codec";
		value = fst_packet_get_str (data, fst_packet_remaining (data));
		break;
	case FILE_TAG_OS:
		name = "operating system";
		value = fst_packet_get_str (data, fst_packet_remaining (data));
		break;

	case FILE_TAG_RATING:
		name = "rating";
		switch (fst_packet_get_dynint (data))
		{
		case 0: value = strdup ("Very poor"); break;
		case 1: value = strdup ("Poor"); break;
		case 2: value = strdup ("OK"); break;
		case 3: value = strdup ("Good"); break;
		case 4: value = strdup ("Excellent"); break;
		default:
			value = NULL;
		}
		break;
	
	case FILE_TAG_HASH:
	case FILE_TAG_SIZE:
	default:
		name = value = NULL;
	}

	if(!value || !name)
		return NULL;

	tag = fst_metatag_create (name, value);
	free (value);
	
	return tag;
}

/*****************************************************************************/
