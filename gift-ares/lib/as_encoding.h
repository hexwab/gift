/*
 * $Id: as_encoding.h,v 1.4 2005/11/15 21:49:23 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_ENCODING_H
#define __AS_ENCODING_H

/*****************************************************************************/

/* caller frees returned string */
char *as_base64_encode (const unsigned char *data, int src_len);

/* caller frees returned string */
unsigned char *as_base64_decode (const char *data, int *dst_len);

/* caller frees returned string */
char *as_hex_encode (const unsigned char *data, int src_len);

/* caller frees returned string */
unsigned char *as_hex_decode (const char *data, int *dst_len);

/* caller frees returned string */
char *as_url_encode (const char *decoded);

/* caller frees returned string */
char *as_url_decode (const char *encoded);

/*****************************************************************************/

#endif /* __AS_ENCODING_H */
