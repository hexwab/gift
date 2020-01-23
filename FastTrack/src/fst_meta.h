/*
 * $Id: fst_meta.h,v 1.9 2004/03/08 21:09:57 mkern Exp $
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

#ifndef __FST_META_H
#define __FST_META_H

#include "fst_packet.h"

/*****************************************************************************/

typedef enum
{
	FILE_TAG_ANY		= 0x00, /* only used for searching */
	FILE_TAG_YEAR		= 0x01,
	FILE_TAG_FILENAME	= 0x02,
    FILE_TAG_HASH		= 0x03, /* only used for searching */
	FILE_TAG_TITLE		= 0x04,
	FILE_TAG_TIME		= 0x05,
    FILE_TAG_ARTIST		= 0x06,
	FILE_TAG_ALBUM		= 0x08,
	FILE_TAG_LANGUAGE	= 0x0a,
	FILE_TAG_KEYWORDS	= 0x0c,
	FILE_TAG_RESOLUTION	= 0x0d,
    FILE_TAG_CATEGORY	= 0x0e,
	FILE_TAG_OS			= 0x10, /* e.g. "Windows 95/98" */
	FILE_TAG_BITDEPTH	= 0x11,
	FILE_TAG_TYPE		= 0x12, /* e.g. "movie", "video clip", "amateur" */
	FILE_TAG_QUALITY	= 0x15,
    FILE_TAG_VERSION	= 0x18,
	FILE_TAG_UNKNOWN1   = 0x19, /* 0x00 and large integer witnessed */
	FILE_TAG_COMMENT	= 0x1a,
	FILE_TAG_CODEC		= 0x1c,
	FILE_TAG_RATING		= 0x1d,
    FILE_TAG_SIZE		= 0x21, /* only used for searching */
	FILE_TAG_UNKNOWN2   = 0x35, /* 0x02 witnessed */
} FSTFileTag;


typedef enum
{
	FILE_TAG_DATA_ANY,
	FILE_TAG_DATA_STRING,
	FILE_TAG_DATA_INT,
	FILE_TAG_DATA_INTINT,
	FILE_TAG_DATA_RAW
} FSTFileTagData;


typedef enum
{
	MEDIA_TYPE_UNKNOWN  = 0x00,
	MEDIA_TYPE_AUDIO    = 0x01,
	MEDIA_TYPE_VIDEO    = 0x02,
	MEDIA_TYPE_IMAGE    = 0x03,
	MEDIA_TYPE_DOCUMENT = 0x04,
	MEDIA_TYPE_SOFTWARE = 0x05
} FSTMediaType;


/* legacy struct still used by fst_search.c */
typedef struct
{
	char *name;					/* human readable tag name */
	char *value;				/* human readable tag value */

} FSTMetaTag;

/*****************************************************************************/

FSTMediaType fst_meta_mediatype_from_mime (const char *mime);

/*****************************************************************************/

/* returns fasttrack file tag from name */
FSTFileTag fst_meta_tag_from_name (const char *name);

/* returns static name string from fasttrack file tag */
char *fst_meta_name_from_tag (FSTFileTag tag);

/*****************************************************************************/

/* returns human readable value string from packet data, caller frees */
char *fst_meta_giftstr_from_packet (FSTFileTag tag, FSTPacket *data);

/* returns packet for one meta tag, caller frees packet */
FSTPacket *fst_meta_packet_from_giftstr (const char *name, const char *value);

/* returns value as used in http replies, caller frees */
char *fst_meta_httpstr_from_giftstr (const char *name, const char *value);

/*****************************************************************************/

/* legacy functions still used by fst_search.c */

/* alloc and init tag */
FSTMetaTag *fst_metatag_create (char *name, char *value);

/* free tag */
void fst_metatag_free (FSTMetaTag *tag);

/* alloc and init tag from protocol data */
FSTMetaTag *fst_metatag_create_from_filetag (FSTFileTag tag, FSTPacket *data);

/*****************************************************************************/

#endif /* __FST_META_H */
