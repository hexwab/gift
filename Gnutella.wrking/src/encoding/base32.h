/*
 * $Id: base32.h,v 1.1 2004/03/05 17:47:29 hipnod Exp $
 *
 * Copyright (C) 2003-2004 giFT project (gift.sourceforge.net)
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

#ifndef GIFT_GT_BASE32_H_
#define GIFT_GT_BASE32_H_

/*****************************************************************************/

BOOL gt_base32_valid  (const char *base32, size_t len);

void gt_base32_encode (const uint8_t *in, size_t in_len,
                       char *out, size_t out_len);

void gt_base32_decode (const char *in, size_t in_len,
                       uint8_t *out, size_t out_len);

/*****************************************************************************/

#endif /* GIFT_GT_BASE32_H_ */
