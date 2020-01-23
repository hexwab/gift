/*
 * $Id: xml.h,v 1.2 2003/11/08 12:29:55 hipnod Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#ifndef GIFT_GT_XML_H_
#define GIFT_GT_XML_H_

/*****************************************************************************/

#define XML_DEBUG               gt_config_get_int("xml/debug=0")

/*****************************************************************************/

BOOL      gt_xml_parse          (const char *xml, Dataset **ret);
BOOL      gt_xml_parse_indexed  (const char *xml, size_t bin_len,
                                 Share **shares, size_t shares_len);

void      gt_xml_init           (void);
void      gt_xml_cleanup        (void);

/*****************************************************************************/

#endif /* GIFT_GT_XML_H_ */
