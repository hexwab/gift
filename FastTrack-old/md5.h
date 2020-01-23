/*
 * Copyright (C) 2003 Markus Kern (mkern@users.sourceforge.net)
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

#define MD5_HASH_LEN 16

typedef struct
{
	unsigned int     buf[4];
	unsigned int     bits[2];
	unsigned char in[64];
} MD5Context;

void MD5Init(MD5Context *context);
void MD5Update(MD5Context *context, unsigned char const *buf, unsigned len);
void MD5Final(unsigned char digest[MD5_HASH_LEN], MD5Context *context);

/*****************************************************************************/

#endif /* __MD5_H */

