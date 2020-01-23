/*
 * $Id: meta_ogg.c,v 1.10 2003/03/20 05:01:12 rossta Exp $
 *
 * inappropriately named, this is actually for parsing the Ogg Vorbis codec
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#include "gift.h"

#include "share_file.h"

#include "meta.h"
#include "meta_ogg.h"

#ifdef USE_LIBVORBIS
#include <vorbis/vorbisfile.h>
#endif

/*****************************************************************************/

int meta_ogg_run (FileShare *file, char *path)
{
#ifdef USE_LIBVORBIS
	FILE           *f;
	OggVorbis_File  oggfile;
	vorbis_info    *vinfo;
	vorbis_comment *vcomment;
	int             i;
	long            bitrate;
	long            freq;
	int             dur;

	if (!(f = fopen (path, "rb")))
	{
		GIFT_ERROR (("can't open %s: %s", path, GIFT_STRERROR ()));
		return FALSE;
	}

	if (ov_open (f, &oggfile, NULL, 0))
	{
		fclose (f);
		return FALSE;
	}

	/* extract basic audio info */
	vinfo = ov_info (&oggfile, -1);

	bitrate = ov_bitrate (&oggfile, -1);
	freq    = vinfo->rate;
	dur     = (int) ov_time_total (&oggfile, -1);

	/* sigh, we have to use kbps */
	if (bitrate > 0)
		meta_set (file, "bitrate", stringf ("%li", (long)bitrate));

	meta_set (file, "frequency", stringf ("%li", freq));
	meta_set (file, "duration",  stringf ("%i", dur));

	/* extract comments */
	vcomment = ov_comment (&oggfile, -1);

	for (i = 0; i < vcomment->comments; i++)
	{
		char *cmt, *cmt0;
		char *key;
		char *val;

		cmt0 = cmt = STRDUP (vcomment->user_comments[i]);

		key = string_sep (&cmt, "=");
		val = cmt;

		/* format the key :) */
		string_lower (key);

		if (key && val)
			meta_set (file, key, val);

		free (cmt0);
	}

	/* ov_clear seems to implicitely close the file... */
	ov_clear (&oggfile);
#else
	GIFT_WARN (("unable to process %s: libvorbis support has been disabled",
	            path));
#endif /* USE_LIBVORBIS */

	return TRUE;
}
