/*
 * gift-fe.h
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

#ifndef __GIFT_H
#define __GIFT_H

/*****************************************************************************/

#include <stdio.h>    /* ahh what the hell, why not? */
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <ctype.h>

/* debugging */
#include <assert.h>
#define trace() fprintf (stderr, "%-15s:%-4i %s(): entered\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);

#include <gtk/gtk.h>

/*****************************************************************************/

/* includes from the daemon */
#include "parse.h"

#include "feconnect.h"
#include "daemon.h"

#include "network.h"

#include "ui_utils.h"

/*****************************************************************************/

#define GIFT_FE_PACKAGE "giFT-fe"
#define GIFT_FE_VERSION "0.10.0"

/*****************************************************************************/

typedef struct
{
	GtkWidget *window;

	GtkWidget *notebook;

	/*
	 * Search page
	 */

	/* [ query__________ ] ( audio__ [v] ) <search> */
	GtkWidget *query;
	GtkWidget *media_filter;
	GtkWidget *search;

	/* main search results list */
	GtkWidget *srch_list;

	/*
	 * Transfer page
	 */

	GtkWidget *dl_frame;
	GtkWidget *dl_list;
	GtkWidget *ul_frame;
	GtkWidget *ul_list;

	/*
	 * Statistics page(s)
	 */

	GtkWidget *stats_list;
} FTApp;

/*****************************************************************************/

#include "ui.h"    /* user interface manipulation */
#include "obj.h"   /* displayed data objects */
#include "utils.h" /* misc utilities */

/*****************************************************************************/

#endif /* __GIFT_H */
