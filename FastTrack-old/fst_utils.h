/*
 * Copyright (C) 2003 Markus Kern (mkern@users.sourceforge.net)
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

#ifndef __UTILS_H
#define __UTILS_H

/*****************************************************************************/

void print_bin_data(unsigned char * data, int len);
void save_bin_data(unsigned char * data, int len);

/*****************************************************************************/

// caller frees returned string
char *fst_utils_url_decode (char *encoded);

// caller frees returned string
char *fst_utils_url_encode (char *decoded);

// returns TRUE if ip in reserved private space, ip is big endian
int fst_utils_ip_private (unsigned int ip);

/*****************************************************************************/

#endif /* __UTILS_H */
