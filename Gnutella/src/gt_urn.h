/*
 * $Id: gt_urn.h,v 1.3 2004/03/05 17:47:29 hipnod Exp $
 *
 * Copyright (C) 2003 giFT project (gift.sourceforge.net)
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

#ifndef GIFT_GT_URN_H_
#define GIFT_GT_URN_H_

/*****************************************************************************/

typedef unsigned char gt_urn_t;

/*****************************************************************************/

gt_urn_t      *gt_urn_new           (const char *algo,
                                     const unsigned char *data);
char          *gt_urn_string        (const gt_urn_t *urn);
gt_urn_t      *gt_urn_parse         (const char *string);
int            gt_urn_cmp           (gt_urn_t *a, gt_urn_t *b);

unsigned char *gt_urn_data          (const gt_urn_t *urn);

/*****************************************************************************/

#endif /* GIFT_GT_URN_H_ */
