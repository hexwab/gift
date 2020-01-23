/*
 * md5.h
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

#ifndef __MD5_H
#define __MD5_H

/* length of hash in bytes, *2 for string length */
#define HASH_LEN 16

#define HASH_STR_LEN (HASH_LEN * 2)

/*****************************************************************************/

/* Calculate hash of size bytes of file and return 16 byte binary hash digest.
 * Return result in digest and return value, otherwise return NULL.
 */
unsigned char *md5_digest (char *file, unsigned long size,
                           unsigned char** digest);

/* Calculate hash of size bytes of file and return 32 bytes hex string of hash
 * digest.
 * Return result in hash_str and return value, otherwise return NULL.
 */
char *md5_string (char *file, unsigned long size, char **hash_str);

/* Calculate hash of size bytes of file and return 32 bytes hex string of hash
 * digest.
 * Return result in hash_str and return value, otherwise return NULL.
 * Return value has been malloc'd.
 */
char *md5_checksum (char *file, unsigned long size);

/* Should we move these functions to a utility (parse.h?) file */

/* Convert binary data to a a null-terminated lower-case hex string.
 * hex must point to a buffer twice as long as len plus 1 byte.
 * Return result in hex and return value.
 */
char *bin_to_hex (unsigned char *bin, char **hex, unsigned long len);

/* Convert null-terminated hex string to binary.
 * bin must point to a buffer half the length of len;
 * Return result in bin and return value.
 */
unsigned char *hex_to_bin (char *hex, unsigned char **bin);

unsigned char *md5_calc_digest (unsigned char *data, unsigned long size,
                                unsigned char** digest);

#endif /* __MD5_H */

