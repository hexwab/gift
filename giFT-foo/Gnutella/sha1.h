/*
 * $Id: sha1.h,v 1.7 2003/05/05 09:04:02 hipnod Exp $
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

#ifndef __GT_SHA1_H__
#define __GT_SHA1_H__

#define SHA1_BINSIZE      20
#define SHA1_STRLEN       32

unsigned char *sha1_parse  (char *str);
char          *sha1_string (unsigned char *sha1);
unsigned char *sha1_digest (const char *file, off_t size);
unsigned char *sha1_bin    (char *ascii);
unsigned char *sha1_dup    (unsigned char *sha1);

#endif /* __GT_SHA1_H__ */
