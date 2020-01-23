/*
 * ed_main.h
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

#ifndef __EDONKEY_H
#define __EDONKEY_H

#define EDONKEY_MAJOR 0
#define EDONKEY_MINOR 0
#define EDONKEY_MICRO 0
#define EDONKEY_REV   1

/*****************************************************************************/

#ifndef LOG_PFX
# define LOG_PFX "[eDonkey ] "
#endif

#include "giftconfig.h"

/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gift.h"
#include "share_file.h"
#include "download.h"
#include "upload.h"
#include "event.h"

#include "dataset.h"
#include "file.h"
#include "conf.h"
#include "share_db.h"

#include "network.h"

/* #include <tcgprof.h> */
#include <assert.h>

/*****************************************************************************/

#ifndef __OPENFT_C__
extern Protocol *edonkey_proto;
#endif

/*****************************************************************************/

/* registered callbacks to report transfer statistics to the daemon */
void ft_upload   (Chunk *chunk, char *segment, size_t len);
void ft_download (Chunk *chunk, char *segment, size_t len);

int  eDonkey_init (Protocol *p);

/*****************************************************************************/

#endif /* __EDONKEY_H */
