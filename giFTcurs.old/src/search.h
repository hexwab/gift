/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * giFTcurs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with giFTcurs; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: search.h,v 1.46 2003/05/14 09:09:35 chnix Exp $
 */
#ifndef _SEARCH_H
#define _SEARCH_H

#include "tree.h"
#include "gift.h"

typedef struct {
	tree hits;
	gift_id id;
	char *search_term;
	int realm;
	int pretty_width;			/* The text width hits are formatted for. */
	list hits_index;			/* hits sorted by hash for speed-up */
	void *formatting;
	int sorting_method;
	unsigned dirty:1;
} query;

typedef struct {
	tree_node tnode;
	list metadata;
	list infobox;
	char *hash;
	unsigned int filesize;
	char *filename;
	char *directory;
	char *mime;
	void *formatting;			/* format audio, video etc. */
	unsigned downloading:1;
} hit;

typedef struct {
	tree_node tnode;
	char *user;					/* The users that have this file */
	char *node;
	char *href;					/* The references to those files */
	unsigned char availability;
} subhit;

typedef struct {
	char *key;
	char *data;
} metadata;

extern const rendering hit_methods, subhit_methods;

extern list queries;

/* this needs to be called upon startup/exit */
void gift_search_init(void);
void gift_search_cleanup(void);

query *new_query(char *search_term, int realm);

/* stop the search associated with id */
void gift_search_stop(gift_id id, const char *type);

/* stop text query, NULL stops all */
void gift_query_stop(query * q);

/* remove all hits from a query */
void gift_query_clear(query * q);

/* free hit list */
void gift_hits_free(query * q);

char *change_sort_method(int direction);

/* Mark hits with this file as currently downloading */
int hit_download_mark(const char *hash, unsigned int size);

void hit_destroy(hit *);
void subhit_destroy(subhit *);

const char *meta_data_lookup(const hit * the_hit, const char *key);

/* This thing parses search results for the UI part. */
void search_result_item_handler(ntree * data, query * q);

/* remove all hits from this user, in current and future searches */
void user_ignore(char *user);

/* check if this user is ignored */
int user_isignored(const char *user);

#endif
