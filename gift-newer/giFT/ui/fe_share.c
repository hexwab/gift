/*
 * fe_share.c
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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

#include "gift-fe.h"

#include "fe_share.h"

/*****************************************************************************/

/* include xpm files */
#include "mime_empty.xpm"
#include "unknown.xpm"
#include "video.xpm"
#include "sound.xpm"
#include "document2.xpm"
#include "pdf.xpm"
#include "html.xpm"
#include "image.xpm"

struct mime_entry {
	char *head;
	char *exact;
	char **xpm;
	GtkWidget *pixmap;
};

static struct mime_entry mimes[] = {
	{"text", "pdf", pdf_xpm, NULL},
	{"text", "htm", html_xpm, NULL},
	{"text", "plain", document2_xpm, NULL},
	{"text", "/", mime_empty_xpm, NULL},
	{"image", "/", image_xpm, NULL},
	{"audio", "/", sound_xpm, NULL},
	{"video", "/", video_xpm, NULL},
	{"", "/", unknown_xpm, NULL},
	{NULL, NULL, NULL, NULL}
};

/*****************************************************************************/

/* share_get_pixmap
 * CALLED by: search_insert, download_insert, upload_insert
 * this function returns the pixmap, bitmap pair for the mime type of a given
 * share if neccessary it is created, and stored in mimes, the array of
 * recognized mime types
 */

int share_get_pixmap (SharedFile *share, GdkColormap *colormap,
					  GdkPixmap **pixmap, GdkBitmap **bitmap)
{
	GtkWidget **gtkpixmap;
	char *mime;
	char **xpm = NULL;
	struct mime_entry *walker;

	assert (share);

	mime = share->mime_type;
	if (mime)
		for (walker=mimes; walker->xpm; walker++)
		{
			if (!strncmp (mime, walker->head, strlen (walker->head)))
				if (strstr (mime, walker->exact))
				{
					xpm = walker->xpm;
					gtkpixmap = &walker->pixmap;
					break;
				}
		}

	assert (xpm);

	if (!(*gtkpixmap))
	{
		/* Create the Pixmap */
		*pixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap,
							bitmap, NULL, xpm);
		*gtkpixmap = gtk_pixmap_new (*pixmap, *bitmap);
	}
	else
	{
		/* use the previous created */
		gtk_pixmap_get (GTK_PIXMAP (*gtkpixmap), pixmap, bitmap);
	}
	return *pixmap ? TRUE : FALSE;
}

static char *guess_mime_type (char *filename)
{
	char *ext;
	/* Remove this evil hack as soon as mime handling is in the daemon */
	ext = strrchr (filename, '.');
	if (ext)
	{
		if (!strcasecmp (ext, ".avi")) return "video/x-avi";
		if (!strcasecmp (ext, ".asf")) return "video/x-asf";
		if (!strcasecmp (ext, ".mpg") || !strcasecmp (ext, ".mpeg"))
			return "video/x-mpeg";
		if (!strcasecmp (ext, ".mp3")) return "audio/x-mpeg";
		if (!strcasecmp (ext, ".ogg")) return "audio/x-ogg";
		if (!strcasecmp (ext, ".wav")) return "audio/x-wav";
		if (!strcasecmp (ext, ".pdf")) return "text/pdf";
		if (!strcasecmp (ext, ".ps")) return "text/postscript";
		if (!strcasecmp (ext, ".txt")) return "text/plain";
		if (!strncasecmp (ext, ".htm", 4)) return "text/html";
		if (!strcasecmp (ext, ".jpg") || !strcasecmp (ext, ".jpeg"))
			return "image/jpeg";
	}
	return "application/octet-stream"; /* dont know */
}

void share_fill_data (SharedFile *share, GData *ft_data)
{
	char *hash;

	if (!share || !ft_data)
		return;

	/* user = 'user@network', network = 'network' */
	share->user = format_user_disp (g_datalist_get_data (&ft_data, "user"));

	if (share->user)
	{
		if ((share->network = strrchr (share->user, '@')))
			share->network++;
	}
	else
		share->network = NULL;

	/*
	 * location = 'http://bla/bla/file%5bthis%5d+that.bla'
	 * filename = 'file[this] that.bla'
	 */

	/*	Protocol change everyone !! now as follows:
		first packet:	second packet:		upload packet:
		id			id			id
		action			user			action
		size			addsource		size
		hash						user
		save						href
	 */

	share->location = NULL;

	if (g_datalist_get_data(&ft_data, "addsource"))
		share->location = STRDUP (g_datalist_get_data (&ft_data, "addsource"));
	else
		share->location = STRDUP (g_datalist_get_data (&ft_data, "href"));

	if (share->location)
		share->filename = format_href_disp(share->location);
	else
		share->filename = STRDUP (g_datalist_get_data (&ft_data, "save"));

	share->filesize = ATOI (g_datalist_get_data (&ft_data, "size"));
	if (share->filename)
		share->mime_type = STRDUP (guess_mime_type (share->filename));

	/* This is true for uploads and the first download packet */
	if ((hash = g_datalist_get_data(&ft_data, "hash")) != NULL)
		share->hash = STRDUP (hash);
}

/* dup for object movement through the list structures ... favored over a
 * reference count system ... */
void share_dup (SharedFile *out, SharedFile *in)
{
	assert (in);
	assert (out);

	out->user = STRDUP (in->user);

	if (out->user)
	{
		if ((out->network = strrchr (out->user, '@')))
			out->network++;
	}
	else
		out->user = NULL;

	out->location = STRDUP (in->location);
	out->filename = STRDUP (in->filename);
	out->mime_type = STRDUP (in->mime_type);
	out->filesize = in->filesize;

	out->hash = STRDUP (in->hash);
}

void fe_share_free (SharedFile *share)
{
	assert (share);

	free (share->user);
	share->network = NULL;

	free (share->location);
	free (share->filename);
	free (share->mime_type);

	share->filesize = 0;

	free (share->hash);

#ifdef DEBUG
	share->user = NULL;
	share->location = NULL;
	share->filename = NULL;
	share->mime_type = NULL;
	share->hash = NULL;
#endif
	/* don't free share, it wasn't dynamically allocated ... */
}
