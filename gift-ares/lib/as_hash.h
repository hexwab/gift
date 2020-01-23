/*
 * $Id: as_hash.h,v 1.6 2005/01/13 19:51:09 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_HASH_H
#define __AS_HASH_H

/*****************************************************************************/

#define AS_HASH_SIZE        (20)  /* SHA_DIGESTSIZE */
#define AS_HASH_BASE64_SIZE (29)  /* base64 len including '\0' */

/* hmm, overkill? */
typedef struct
{
	as_uint8 data[AS_HASH_SIZE];

} ASHash;

/*****************************************************************************/

/* create hash from raw data */
ASHash *as_hash_create (const as_uint8 *src, unsigned int src_len);

/* create copy of hash */
ASHash *as_hash_copy (ASHash *hash);

/* create hash from file */
ASHash *as_hash_file (const char *file);

/* free hash */
void as_hash_free (ASHash *hash);

/*****************************************************************************/

/* return TRUE if the hashes are equal */
as_bool as_hash_equal (ASHash *a, ASHash *b);

/*****************************************************************************/

/* create hash from base64 encoded string */
ASHash *as_hash_decode (const char *encoded);

/* return base64 encoded string of hash. caller frees result. */
char *as_hash_encode (ASHash *hash);

/* Return static string of hash for _debugging_ purposes. Do not use for
 * anything critical because threading may corrupt buffer. */
char *as_hash_str (ASHash *hash);

/*****************************************************************************/

#endif /* __AS_HASH_H */

