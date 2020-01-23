/*
 * $Id: fst_hash.h,v 1.11 2004/03/10 02:07:01 mkern Exp $
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

#ifndef HASH_TEST

#include "fst_fasttrack.h"

typedef fst_uint8  uint8;
typedef fst_uint16 uint16;
typedef fst_uint32 uint32;

#else

#include <ctype.h>
#define TRUE 1
#define FALSE 0

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;
typedef int            BOOL;

#endif

#include "md5.h"

/*****************************************************************************/

/*
 * Legend:
 *
 * md5300     The MD5 hash of the first 300K of the file
 * smallhash  The 4 byte sparse hash of the file
 * md5tree    The new md5 tree of the entire file
 * fthash     The concatenation of md5300 + smallhash
 * kzhash     The concatenation of fthash + md5tree
 * checksum   Two byte checksum of fthash
 */

/* magnet kzhash (hex) */
#define FST_KZHASH_LEN		36
#define FST_KZHASH_STR_LEN	72   /* length of hex string */
#define FST_KZHASH_NAME     "kzhash"

/* sig2dat hash (base64) */
#define FST_FTHASH_LEN		20
#define FST_FTHASH_STR_LEN	29   /* length of base64 string plus leading '=' */
#define FST_FTHASH_NAME     "FTH"

#define FST_KZHASH(h)    ((uint8 *)h->data)
#define FST_FTHASH(h)    ((uint8 *)h->data)
#define FST_MD5300(h)    ((uint8 *)h->data)
#define FST_SMALLHASH(h) ((uint8 *)h->data + 16)
#define FST_MD5TREE(h)   ((uint8 *)h->data + 20)
#define FST_CHECKSUM(h)  (fst_hash_checksum (h))

#define FST_FTSEG_SIZE 307200 /* segment size of smallhash */
#define FST_KZSEG_SIZE 32768  /* segment size of md5 tree*/

/*****************************************************************************/

typedef struct
{
	/* md5300 */
	MD5Context md5300_ctx;
	
	/* smallhash */
	uint32 smallhash;
	uint32 prev_smallhash;
	size_t pos;
	size_t sample_pos;           /* next position we calculate smallhash */
	size_t wnd_start;            /* position in wnd where our window starts */
	uint8 wnd[FST_FTSEG_SIZE];   /* rolling window of FST_FTSEG_SIZE */

	/* md5tree */
	MD5Context md5tree_ctx;
	uint8 nodes[MD5_HASH_LEN * 32]; /* space for file sizes up to 2^32 */
	size_t index;                   /* current position in nodes */
	size_t blocks;                  /* the number of segments we hashed */

} FSTHashContext;

typedef struct
{
	uint8 data[FST_KZHASH_LEN];

	/* used during hash calculation, do not use externally */
	FSTHashContext *ctx;
} FSTHash;

/*****************************************************************************/

/* our primary hash is the kzhash... */
unsigned char *fst_giftcb_kzhash (const char *path, size_t *len);

char *fst_giftcb_kzhash_encode (unsigned char *data);

/* ...but we also do download verification using fthash */
unsigned char *fst_giftcb_fthash (const char *path, size_t *len);

char *fst_giftcb_fthash_encode (unsigned char *data);

/*****************************************************************************/

/* create new hash object */
FSTHash *fst_hash_create ();

/* create new hash object from org_hash */
FSTHash *fst_hash_create_copy (FSTHash *org_hash);

/* create new hash object from raw data */
FSTHash *fst_hash_create_raw (const uint8 *data, size_t len);

/* free hash object */
void fst_hash_free (FSTHash *hash);

/* set from raw data */
BOOL fst_hash_set_raw (FSTHash* hash, const uint8 *data, size_t len);

/* returns TRUE if md5tree is present, FALSE otherwise */
BOOL fst_hash_have_md5tree (FSTHash *hash);

/* returns TRUE if the hashes are equivalent. If one of the hashes doesn't
 * have the md5tree only the fthash is compared.
 */
BOOL fst_hash_equal (FSTHash *a, FSTHash *b);

/* returns checksum of fzhash */
uint16 fst_hash_checksum (FSTHash *hash);

/*****************************************************************************/

/* clear hash and prepare for updates */
void fst_hash_init (FSTHash *hash);

/* update hash */
void fst_hash_update (FSTHash *hash, const uint8 *data, size_t inlen);

/* finish hashing */
void fst_hash_finish (FSTHash *hash);

/* hash entire file */
BOOL fst_hash_file (FSTHash *hash, const char *file);

/*****************************************************************************/

/* parse hex encoded kzhash */
BOOL fst_hash_decode16_kzhash (FSTHash *hash, const char *kzhash);

/* parse hex encoded fthash */
BOOL fst_hash_decode16_fthash (FSTHash *hash, const char *fthash);

/* parse base64 encoded kzhash */
BOOL fst_hash_decode64_kzhash (FSTHash *hash, const char *kzhash);

/* parse base64 encoded fthash */
BOOL fst_hash_decode64_fthash (FSTHash *hash, const char *fthash);

/*****************************************************************************/

/* return pointer to static, hex encoded kzhash string */
char *fst_hash_encode16_kzhash (FSTHash *hash);

/* return pointer to static, hex encoded fthash string */
char *fst_hash_encode16_fthash (FSTHash *hash);

/* return pointer to static, base64 encoded kzhash string */
char *fst_hash_encode64_kzhash (FSTHash *hash);

/* return pointer to static, base64 encoded fthash string */
char *fst_hash_encode64_fthash (FSTHash *hash);

/*****************************************************************************/

/*
 * Updates 4 byte small hash that is concatenated to the md5 of the first
 * 307200 bytes of the file. Set hash to 0xFFFFFFFF for first run.
 * This is exposed here because it is needed for participation level
 * calculation in download.c
 */
uint32 fst_hash_small (uint32 smallhash, const uint8 *data, size_t len);

/*****************************************************************************/

#endif /* __FST_HASH_H */
