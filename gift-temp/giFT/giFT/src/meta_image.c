/*
 * $Id: meta_image.c,v 1.10 2003/04/12 03:27:42 jasta Exp $
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

#include "gift.h"

#include "share_file.h"

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

	meta_set (file, "width",  stringf ("%i", (int) img->columns));
	meta_set (file, "height", stringf ("%i", (int) img->rows));

	DestroyImage (img);
	DestroyImageInfo (info);
	return TRUE;

}
#endif /* USE_IMAGEMAGIC */

int meta_image_run (FileShare *file, char *path)
{
#ifdef USE_IMAGEMAGICK
	ExceptionInfo excp;

	/* initialize */
	InitializeMagick (NULL);
	GetExceptionInfo (&excp);

	/* set the info giFT cares about */
	set_image_info (file, path, &excp);

	/* clean up */
	DestroyExceptionInfo (&excp);
	DestroyMagick ();
#endif /* USE_IMAGEMAGICK */

	return TRUE;
}
