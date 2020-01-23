/*
 * $Id: meta_image.c,v 1.3 2003/06/28 08:42:01 jasta Exp $
 *
 * submitted by: asm@dtmf.org
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

#include "giftd.h"

#include "plugin/share.h"

#include "meta.h"
#include "meta_image.h"

#ifdef USE_IMAGEMAGICK
#include <magick/api.h>
#endif

/*****************************************************************************/

#ifdef USE_IMAGEMAGICK
static int set_image_info (FileShare *file, char *path, ExceptionInfo *excp)
{
	Image     *img;
	ImageInfo *info;

	/* create context */
	if (!(info = CloneImageInfo (NULL)))
		return FALSE;

	strcpy (info->filename, path);

	/* open image */
	if (!(img = ReadImage (info, excp)))
	{
		GIFT_ERROR (("ImageMagick can't process image %s: %s (%s)",
		             info->filename, excp->reason, excp->description));
		DestroyImageInfo (info);
		return FALSE;
	}

	share_set_meta (file, "width",  stringf ("%i", (int)img->columns));
	share_set_meta (file, "height", stringf ("%i", (int)img->rows));

	DestroyImage (img);
	DestroyImageInfo (info);
	return TRUE;

}
#endif /* USE_IMAGEMAGIC */

/************************************************************/

#define BYTE(var) { int byte=fgetc(fh); if (byte==EOF) return 0; var=byte; }
#define WORD(var) { int j,k; BYTE(j); BYTE(k); var=(j<<8)+k; }

/*
 * JPEG markers consist of one or more 0xFF bytes, followed by a marker
 * code byte (which is not an FF).  Here are the marker codes of interest
 * in this program.  (See jdmarker.c for a more complete list.)
 */

#define M_SOF0  0xC0		/* Start Of Frame N */
#define M_SOF1  0xC1		/* N indicates which compression process */
#define M_SOF2  0xC2		/* Only SOF0-SOF2 are now in common use */
#define M_SOF3  0xC3
#define M_SOF5  0xC5		/* NB: codes C4 and CC are NOT SOF markers */
#define M_SOF6  0xC6
#define M_SOF7  0xC7
#define M_SOF9  0xC9
#define M_SOF10 0xCA
#define M_SOF11 0xCB
#define M_SOF13 0xCD
#define M_SOF14 0xCE
#define M_SOF15 0xCF
#define M_SOI   0xD8		/* Start Of Image (beginning of datastream) */
#define M_EOI   0xD9		/* End Of Image (end of datastream) */
#define M_SOS   0xDA		/* Start Of Scan (begins compressed data) */


static BOOL
skip_variable (FILE *fh)
/* Skip over an unknown or uninteresting variable-length marker */
{
	unsigned int length;

	/* Get the marker parameter length count */
	WORD (length);

	/* Length includes itself, so must be at least 2 */
	if (length < 2)
		return 0;

	length -= 2;

	/* Skip over the remaining bytes */
	if (fseek (fh, length, SEEK_CUR))
		return FALSE;

	return TRUE;
}

/*
 * Process a SOFn marker.
 * This code is only needed if you want to know the image dimensions...
 */

static BOOL
process_SOFn (FILE *fh, Share *file)
{
	unsigned int length;
	unsigned int height, width;
	int data_precision, num_components;

	WORD (length);	/* usual parameter length count */

	BYTE (data_precision);
	WORD (height);
	WORD (width);
	BYTE (num_components);

	if (length != (unsigned int) (8 + num_components * 3))
		return FALSE;

	share_set_meta (file, "width",  stringf ("%u", width));
	share_set_meta (file, "height", stringf ("%u", height));

	return TRUE;
}

static BOOL
get_jpeg_info (Share *share, const char *path)
{
	FILE *fh = fopen (path, "rb");

	if (!fh)
		return FALSE;

	if (fgetc (fh) != 0xff)
		return FALSE;

	if (fgetc (fh) != M_SOI)
		return FALSE;

	/* Scan miscellaneous markers until we reach SOS. */
	for (;;)
	{
		int c;

		/* Find 0xFF byte; count and skip any non-FFs. */
		BYTE(c);
		while (c != 0xFF) {
			BYTE(c);
		}

		/* Get marker code byte, swallowing any duplicate FF bytes.  Extra FFs
		 * are legal as pad bytes, so don't count them in discarded_bytes.
		 */
		do {
			BYTE(c);
		} while (c == 0xFF);
    
		switch (c)
		{
		case M_SOF0: case M_SOF1: case M_SOF2:
		case M_SOF3: case M_SOF5: case M_SOF6:
		case M_SOF7: case M_SOF9: case M_SOF10:
		case M_SOF11:case M_SOF13:case M_SOF14:
		case M_SOF15:
			return process_SOFn (fh, share);
			break;

		case M_SOS:
		case M_EOI:
			return TRUE;
			break;

		default:
			if (!skip_variable (fh))
				return FALSE;
			break;
		}
	}
}

BOOL meta_image_run (Share *share, const char *path)
{
#ifdef USE_IMAGEMAGICK
	ExceptionInfo excp;

	/* initialize */
	InitializeMagick (NULL);
	GetExceptionInfo (&excp);

	/* set the info giFT cares about */
	set_image_info (share, (char *)path, &excp);

	/* clean up */
	DestroyExceptionInfo (&excp);
	DestroyMagick ();
#else
	get_jpeg_info (share, path);
#endif /* USE_IMAGEMAGICK */

	return TRUE;
}
