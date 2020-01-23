/*
 * ft_utils.h
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

#ifndef __UTILS_H
#define __UTILS_H

/*****************************************************************************/

void  ft_accept_test     (Connection *c);
#if 0
int   ft_connect         (Connection *c);
void  ft_handle_incoming (Protocol *p, Connection *c);
#endif

/*****************************************************************************/

int validate_share_submit ();
int parents_needed ();

/*****************************************************************************/

char *node_class_str (unsigned short klass);

/*****************************************************************************/

#endif /* __UTILS_H */
