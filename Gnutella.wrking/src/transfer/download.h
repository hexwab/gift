/*
 * $Id: download.h,v 1.1 2004/01/18 05:42:55 hipnod Exp $
 *
 * Copyright (C) 2004 giFT project (gift.sourceforge.net)
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

#ifndef GIFT_GT_DOWNLOAD_H_
#define GIFT_GT_DOWNLOAD_H_

/*****************************************************************************/

void       gt_download_add      (Transfer *transfer, Source *source);
void       gt_download_remove   (Transfer *transfer, Source *source);

Source    *gt_download_lookup   (const char *sha1);

/*****************************************************************************/

#endif /* GIFT_GT_DOWNLOAD_H_ */
