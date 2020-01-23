/*
 * $Id: trie.h,v 1.2 2003/07/09 09:31:52 hipnod Exp $
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

#ifndef __GT_TRIE_H__
#define __GT_TRIE_H__

/*****************************************************************************/

typedef struct trie
{
	List           *children;               /* List of children.
	                                           The zeroth item is the data list
	                                           if this is a terminal node */
	unsigned char   terminal_node : 1;      /* TRUE if is a terminal node */
	char            c;                      /* Character of this node */

} Trie;

/*****************************************************************************/

Trie       *trie_new      (void);
void        trie_free     (Trie *trie);
void       *trie_lookup   (Trie *trie, char *s);
void        trie_insert   (Trie *trie, char *s, void *value);
void        trie_remove   (Trie *trie, char *s);
void        trie_print    (Trie *trie);

/*****************************************************************************/

#endif /* __GT_TRIE_H__ */
