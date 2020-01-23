/*
 * search.h
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

#ifndef __SEARCH_H
#define __SEARCH_H

/*****************************************************************************/

enum
{
	SEARCH_FILENAME = 0x01,
	SEARCH_MD5      = 0x02,
	SEARCH_HOST     = 0x04,
	SEARCH_LOCAL    = 0x08
};

typedef struct _search
{
	IFEventID  id;  /* the hash table this data structure is stored in is
						 * keyed by id, so this is simply a convenience */

	int        ref; /* how many servers this search relies on */

	int        type;
	char      *query;
	char      *exclude;
	char      *realm;
	size_t     size_min;
	size_t     size_max;
	size_t     kbps_min;
	size_t     kbps_max;
	ft_uint32 *qtokens;
	ft_uint32 *etokens;
} Search;

/*****************************************************************************/

ft_uint32 *search_tokenize  (char *string);
List      *ft_search        (size_t *r_size, int search_type,
                             char *query, char *exclude, char *realm,
                             size_t size_min, size_t size_max,
                             size_t kbps_min, size_t kbps_max);

/*****************************************************************************/

Search    *search_new       (IFEventID id, int search_type, char *query,
                             char *exclude, char *realm,
                             size_t size_min, size_t size_max,
                             size_t kbps_min, size_t kbps_max);
void       search_reply     (IFEventID id, Connection *prnt, FileShare *file);

/*****************************************************************************/

#endif /* __SEARCH_H */
