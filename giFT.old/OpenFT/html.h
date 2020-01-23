/*
 * html.h
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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

#ifndef __HTML_H
#define __HTML_H

/*****************************************************************************/

char *html_strip (char *html);

char *url_encode (char *url);
char *url_decode (char *url);

void  html_update_nodepage ();
char *html_page_index    (char *file);
char *html_page_redirect (char *file);

/*****************************************************************************/

#endif /* __HTML_H */
