/*
 * $Id: md5.h,v 1.4 2003/05/30 02:26:52 jasta Exp $
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

#ifndef __MD5_H
#define __MD5_H

/*****************************************************************************/

unsigned char *md5_digest   (const char *file, off_t size);
char          *md5_checksum (const char *file, off_t size);

unsigned char *md5_dup      (unsigned char *md5);
char          *md5_string   (unsigned char *md5);
unsigned char *md5_bin      (char *md5_ascii);

/*****************************************************************************/

#endif /* __MD5_H */

