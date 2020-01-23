/*
 * $Id: as_share.h,v 1.7 2004/11/27 22:30:37 hex Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

/***********************************************************************/

typedef struct
{
	char    *path; /* full path */
	char    *ext;  /* pointer to within path */
	size_t   size;
	ASHash  *hash;
	ASRealm  realm;
	ASMeta  *meta;
	as_bool  fake; /* whether we have fake metadata */
	void    *udata;
} ASShare;

/***********************************************************************/

/* Create new share object. Takes ownership of hash and meta. If hash
 * is NULL the file will be hashed synchronously before return. */
ASShare *as_share_create (char *path, ASHash *hash, ASMeta *meta,
                          size_t size, ASRealm realm);

/* Create copy of share object. */
ASShare *as_share_copy (ASShare *share);

/* Free share object. */
void as_share_free (ASShare *share);

/* Return packet of share for sending to supernodes. */
ASPacket *as_share_packet (ASShare *share);

/***********************************************************************/
