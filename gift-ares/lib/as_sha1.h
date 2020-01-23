/*
 * $Id: as_sha1.h,v 1.2 2004/09/02 19:39:52 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_SHA1_H_
#define __AS_SHA1_H_

/*****************************************************************************/

#define SHA_BLOCKSIZE		64
#define SHA_DIGESTSIZE		20

#define SHA1_BINSIZE        20
#define SHA1_STRLEN         32

/*****************************************************************************/

typedef struct sha1_state_t
{
	unsigned long  digest[5];           /* message digest */
	unsigned long  count_lo, count_hi;  /* 64-bit bit count */
	as_uint8       data[SHA_BLOCKSIZE]; /* SHA data buffer */
	int            local;               /* unprocessed amount in data */

	/* tranform function specified by initalization */
	void (*transform_fn)(struct sha1_state_t *sha_info);

} ASSHA1State;

/*****************************************************************************/

void as_sha1_init   (ASSHA1State *state);
void as_sha1_update (ASSHA1State *state, const void *data, unsigned int len);
void as_sha1_final  (ASSHA1State *state, unsigned char *hash);

/* special Ares version with different constants and init vectors. */
void as_sha1_ares_init (ASSHA1State *state);

/*****************************************************************************/

#endif /* __AS_SHA1_H_ */
