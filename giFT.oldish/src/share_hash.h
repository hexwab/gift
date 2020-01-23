/*
 * share_hash.h
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

#ifndef __SHARE_HASH_H
#define __SHARE_HASH_H

/*****************************************************************************/

typedef unsigned char* (*HashAlgorithm) (char *path, char *type, int *len);
typedef char* (*HashHuman) (unsigned char *hash, int *len);

typedef struct
{
	unsigned char *hash;
	int            len;
} ShareHash;

typedef struct
{
	char          *type;
	HashAlgorithm  algo;
	HashHuman      human;
} ShareHashType;

/*****************************************************************************/

int  hash_algo_register   (struct _protocol *p, char *type,
                           HashAlgorithm algo, HashHuman human);
void hash_algo_unregister (struct _protocol *p, char *type);
int  hash_algo_run        (struct _file_share *file);

ShareHash *share_hash_new  (unsigned char *hash, int len);
ShareHash *share_hash_dup  (ShareHash *sh);
void       share_hash_free (ShareHash *sh);

ShareHash *share_hash_get   (struct _file_share *file, char *hash_type);
int        share_hash_set   (struct _file_share *file, char *hash_type,
                             unsigned char *hash, int len);
void       share_hash_clear (struct _file_share *file);

/*****************************************************************************/

#endif /* __SHARE_HASH_H */
