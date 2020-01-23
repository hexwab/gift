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

/* define this if you wish to use SEARCH_HIDDEN (documented below) */
/* #define SEARCH_PARANOID */

/*****************************************************************************/

enum
{
	SEARCH_FILENAME = 0x01,
	SEARCH_MD5      = 0x02,
	SEARCH_HOST     = 0x04,
	SEARCH_LOCAL    = 0x08,
	SEARCH_HIDDEN   = 0x10  /* the HIDDEN flag indicates that the human
	                         * readable search string will be substituted
	                         * by the hashed/tokenized query...this is up to
	                         * the implementation how paranoid they wish to
	                         * be ;) */
};

typedef struct _search
{
	IFEventID  id;  /* the hash table this data structure is stored in is
	                 * keyed by id, so this is simply a convenience */

	List      *ref; /* list of search nodes which were queried (and still
					 * need to respond) */

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
                             void *query, void *exclude, char *realm,
                             size_t size_min, size_t size_max,
                             size_t kbps_min, size_t kbps_max);

/*****************************************************************************/

Search    *search_new       (IFEventID id, int search_type, char *query,
                             char *exclude, char *realm,
                             size_t size_min, size_t size_max,
                             size_t kbps_min, size_t kbps_max);
void       search_reply     (IFEventID id, Connection *prnt, FileShare *file);

/*****************************************************************************/

void       search_foreach   (HashFunc func, void *udata, int rem);

/*****************************************************************************/

#endif /* __SEARCH_H */
