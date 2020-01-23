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

#ifndef __FST_META_H
#define __FST_META_H

#include "fst_packet.h"

/*****************************************************************************/

typedef enum
{
	FILE_TAG_ANY		= 0x00,
	FILE_TAG_YEAR		= 0x01,
	FILE_TAG_FILENAME	= 0x02,
    FILE_TAG_HASH		= 0x03,
	FILE_TAG_TITLE		= 0x04,
	FILE_TAG_TIME		= 0x05,
    FILE_TAG_ARTIST		= 0x06,
	FILE_TAG_ALBUM		= 0x08,
	FILE_TAG_LANGUAGE	= 0x0a,
	FILE_TAG_KEYWORDS	= 0x0c,
	FILE_TAG_RESOLUTION	= 0x0d,
    FILE_TAG_GENRE		= 0x0e,
	FILE_TAG_BITDEPTH	= 0x11,
	FILE_TAG_QUALITY	= 0x15,
    FILE_TAG_VERSION	= 0x18,
	FILE_TAG_COMMENT	= 0x1a,
	FILE_TAG_RATING		= 0x1d,
    FILE_TAG_SIZE		= 0x21,

	FILE_TAG_TYPE		= 0x12, // "movie", "video clip", "amateur", ...
	FILE_TAG_CODEC		= 0x1c, // "divx"
	FILE_TAG_OS			= 0x10, // "Windows 95/98"
} FSTFileTag;

typedef struct
{
	char *name;			// human readable tag name
	char *value;		// human readable tag value
} FSTMetaTag;

/*****************************************************************************/

// alloc and init tag
FSTMetaTag *fst_metatag_create (char *name, char *value);

// free tag
void fst_metatag_free (FSTMetaTag *tag);

// alloc and init tag from protocol data
FSTMetaTag *fst_metatag_create_from_filetag (FSTFileTag filetag, FSTPacket *data);

/*****************************************************************************/

#endif /* __FST_META_H */
