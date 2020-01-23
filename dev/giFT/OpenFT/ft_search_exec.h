/*
 * ft_search_exec.h
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

#ifndef __SEARCH_EXEC_H
#define __SEARCH_EXEC_H

/*****************************************************************************/

ft_uint32 *ft_search_tokenize (char *str);
ft_uint32 *ft_search_tokenizef (FileShare *file);

List *ft_search (size_t *matches, FTSearchType type, char *realm,
                 void *query, void *exclude);
int ft_search_cmp (FileShare *file, FTSearchType type, char *realm,
				   void *query, void *exclude);

/*****************************************************************************/

#endif /* __SEARCH_EXEC_H */
