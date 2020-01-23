/*
 * $Id: asp_plugin.h,v 1.3 2004/12/18 23:55:59 mkern Exp $
 *
 * Copyright (C) 2003 giFT-Ares project
 * http://developer.berlios.de/projects/gift-ares
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

#ifndef __ASP_PLUGIN_H
#define __ASP_PLUGIN_H

/*****************************************************************************/

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#ifndef GIFT_PLUGIN
# error "GIFT_PLUGIN not defined. Your build environment seems broken."
#endif

/* Make assumptions that we really shouldn't in order to work around
 * assumptions that giFT really shouldn't make either.
 */
#define EVIL_HASHMAP

/* Get ares lib and libgift headers. */
#include "as_ares.h"

/* Plugin headers. */
#include "asp_hash.h"
#include "asp_search.h"
#include "asp_download.h"
#include "asp_upload.h"
#include "asp_share.h"
#include "asp_hashmap.h"

/*****************************************************************************/

/* Upper bound on number of sessions specifiably in config file. */
#define ASP_MAX_SESSIONS 10

/*****************************************************************************/

/* Pointer to our protocol struct provided by giFT. */
extern Protocol *gift_proto;
#define PROTO (gift_proto)

/*****************************************************************************/

/* Called by gift to init plugin */
#ifdef GIFT_PLUGIN_EXPORT
GIFT_PLUGIN_EXPORT
#endif
  int Ares_init (Protocol *p);

/*****************************************************************************/

#endif /* __ASP_PLUGIN_H */
