/*
 * $Id: dns.h,v 1.1 2003/12/22 01:57:51 hipnod Exp $
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

#ifndef GIFT_GT_DNS_H_
#define GIFT_GT_DNS_H_

/*****************************************************************************/

/*
 * This depends on the right header file being included before this file.
 */
#ifndef NO_ADDRESS
#define NO_ADDRESS NO_DATA
#endif

/*****************************************************************************/

struct hostent *gt_dns_lookup      (const char *name);

int             gt_dns_get_errno   (void);
void            gt_dns_set_errno   (int error_code);
const char     *gt_dns_strerror    (int error_code);

/*****************************************************************************/

#endif /* GIFT_GT_DNS_H_ */
