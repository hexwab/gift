/*
 * $Id: as_download_state.h,v 1.1 2004/09/15 22:46:04 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_DOWNLOAD_STATE_H
#define __AS_DOWNLOAD_STATE_H

/*****************************************************************************/

typedef enum
{
	DOWNSTATE_TAG_KEYWORDS  = 0x01, /* string */
	DOWNSTATE_TAG_TITLE     = 0x02, /* string */
	DOWNSTATE_TAG_ARTIST    = 0x03, /* string */
	DOWNSTATE_TAG_ALBUM     = 0x04, /* string */
	DOWNSTATE_TAG_GENRE     = 0x05, /* string */
	DOWNSTATE_TAG_YEAR      = 0x06, /* string */
	DOWNSTATE_TAG_UNKNOWN_1 = 0x08,
	DOWNSTATE_TAG_COMMENT_1 = 0x09, /* string */
	DOWNSTATE_TAG_COMMENT_2 = 0x0A, /* string */
	DOWNSTATE_TAG_SOURCES   = 0x0D, /* sequence of 17 byte per source */
	DOWNSTATE_TAG_HASH      = 0x0F, /* 20 bytes sha1 of complete file */
	DOWNSTATE_TAG_UNKNOWN_2 = 0x13,
} ASDownStateTags;

/*****************************************************************************/

/* Load state data from file and update download. */
as_bool as_downstate_load (ASDownload *dl);

/* Save state data to end of file being downloaded. */
as_bool as_downstate_save (ASDownload *dl);

/*****************************************************************************/

#endif /* __AS_DOWNLOAD_STATE_H */

