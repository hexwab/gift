/*
 * $Id: fst_hash.h,v 1.5 2003/07/04 03:54:45 beren12 Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#ifndef __FST_HASH_H
#define __FST_HASH_H

#include "md5.h"

/*****************************************************************************/

#define FST_HASH_LEN		20			/* length of binary hash */
#define FST_HASH_STR_LEN	40			/* length of string representing hash without terminating '\0' */
#define FST_HASH_NAME	"FTH"		/* name of hash */
#define FST_HASH_CHUNK	307200		/* size of portion of file that is hashed */

/*****************************************************************************/

unsigned char *gift_cb_FTH (const char *path, size_t *len);

char *gift_cb_FTH_human (unsigned char *FTH);

/*****************************************************************************/

/* hash file */
int fst_hash_file (unsigned char *fth, char *file);

/*
 * updates 4 byte small hash that is concatenated to the md5 of the first
 * 307200 bytes of the file. set hash to 0xffffffff for first run
 */
unsigned int fst_hash_small (unsigned char* data, unsigned int len, unsigned int smallhash);

/* produce 2 byte checksum used in the URL from 20 byte hash */
unsigned short fst_hash_checksum (unsigned char *hash);

/*****************************************************************************/

/*
 * creates human readable string of hash
 * returned string is only valid till next call of function
 */
char *fst_hash_get_string (unsigned char *hash);

/* sets hash from human readable string, */
int fst_hash_set_string (unsigned char *hash, char *string);

/*****************************************************************************/

#endif /* __FST_HASH_H */
