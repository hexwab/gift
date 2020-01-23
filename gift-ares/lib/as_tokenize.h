/*
 * $Id: as_tokenize.h,v 1.3 2004/09/15 21:35:26 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_TOKENIZE_H
#define __AS_TOKENIZE_H

/*****************************************************************************/

/* Tokenize str and add it to search packet. Returns number of added tokens */
int as_tokenize_search (ASPacket *packet, unsigned char *str);

/* Tokenize str and add it to share packet. Returns number of added tokens */
int as_tokenize (ASPacket *packet, unsigned char *str, int type);

/*****************************************************************************/

#endif /* __AS_TOKENIZE_H */
