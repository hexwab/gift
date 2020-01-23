/*
 * $Id: sha1.h,v 1.10 2004/03/05 17:47:29 hipnod Exp $
 *
 * Copyright (C) 2001-2004 giFT project (gift.sourceforge.net)
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

#ifndef GIFT_GT_SHA1_H_
#define GIFT_GT_SHA1_H_

/*****************************************************************************/

#define SHA_BLOCKSIZE		64
#define SHA_DIGESTSIZE		20

#define SHA1_BINSIZE        20
#define SHA1_STRLEN         32

/*****************************************************************************/

struct sha1_state
{
	unsigned long  digest[5];           /* message digest */
	unsigned long  count_lo, count_hi;  /* 64-bit bit count */
	uint8_t        data[SHA_BLOCKSIZE]; /* SHA data buffer */
	int            local;               /* unprocessed amount in data */
};

typedef struct sha1_state sha1_state_t;

/*****************************************************************************/

/* TODO: prefix these suckers */
char          *sha1_string (const unsigned char *sha1);
unsigned char *sha1_digest (const char *file, off_t size);
unsigned char *sha1_bin    (const char *ascii);
unsigned char *sha1_dup    (const unsigned char *sha1);

/*****************************************************************************/

void        gt_sha1_init   (sha1_state_t *state);
void        gt_sha1_append (sha1_state_t *state, const void *data,
                            size_t len);
void        gt_sha1_finish (sha1_state_t *state, unsigned char *hash);

/*****************************************************************************/

#endif /* GIFT_GT_SHA1_H_ */
