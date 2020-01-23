/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001-2002 Göran Weinholt <weinholt@linux.nu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: search.h,v 1.37 2002/11/06 11:46:44 chnix Exp $
 */
#ifndef _SEARCH_H
#define _SEARCH_H

#include "tree.h"
#include "ui_draw.h"
#include "gift.h"
#include "poll.h"

typedef struct {
	rendering *methods;			/* How to draw this item, must be first */
	list sources;				/* list of subhits */
	list metadata;
	list infobox;
	char *hash;
	unsigned int filesize;
	char *filename;
	char *directory;
	char *mime;
	char *pretty;
	void *formatting;			/* format audio, video etc. */
	unsigned downloading:1;
	unsigned expanded:1;
} hit;

typedef struct {
	rendering *methods;			/* How to draw this item, must be first */
	char *user;					/* The users that have this file */
	char *node;
	char *href;					/* The references to those files */
	char *pretty;
	unsigned char availability;
	hit *parent;
} subhit;

typedef struct {
	char *key;
	char *data;
} metadata;

extern rendering hit_methods, subhit_methods;

typedef struct {
	gift_id id;
	char *search_term;
	int realm;
	tree hits;
	list hits_index;			/* hits sorted by hash for speed-up */
	unsigned dirty:1;
} query;

extern list queries;

/* this needs to be called upon startup/exit */
void gift_search_init(void);
void gift_search_cleanup(void);

query *new_query(char *search_term, int realm);

/* stop the search associated with id */
void gift_search_stop(gift_id id, char *type);

/* stop text query, NULL stops all */
void gift_query_stop(query * q);

/* remove all hits from a query */
void gift_query_clear(query * q);

/* free hit list */
void gift_hits_free(query * q);

char *change_sort_method(int direction);

/* Mark hits with this file as currently downloading */
int hit_download_mark(char *hash, unsigned int size);

void hit_destroy(hit *);
void subhit_destroy(subhit *);

typedef int (*ItemCallback) (ntree *, gift_id, void *);
void item_handler_register(gift_id, ItemCallback, void *udata);

char *meta_data_lookup(hit * the_hit, char *key);

/* This thing parses search results for the UI part. */
void search_result_item_handler(ntree * data, query * q);

/* This return the default sorting method, depending on user_browse */
CmpFunc default_cmp_func(int user_browse);

#endif
