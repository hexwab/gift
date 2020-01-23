/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * $Id: transfer.h,v 1.57 2003/06/27 11:20:14 weinholt Exp $
 */
#ifndef _TRANSFER_H
#define _TRANSFER_H

#include "tree.h"
#include "gift.h"

/* struct that keeps track of the latest changes in bandwidth */
struct bandwidth_meas {
	struct {
		int dt;
		int dsize;
	} samples[5];				/* approx. seconds to take average on */
	unsigned int bandwidth;
};

enum transfer_state {
	SOURCE_PAUSED,				/* source has been explicitly paused by user */
	SOURCE_QUEUED_REMOTE,		/* protocol says the other end is preventing us
								 * from downloading */
	SOURCE_QUEUED_LOCAL,		/* we are preventing ourselves from downloading
								 * this file */
	SOURCE_COMPLETE,			/* last known event was that the chunk associated
								 * complete successfully, and is now moving onto
								 * another */
	SOURCE_CANCELLED,			/* remote end cancelled an active transfer */
	SOURCE_TIMEOUT,				/* date timeout */
	SOURCE_WAITING,				/* asked the protocol to download but haven't
								 * received any status back */
	SOURCE_ACTIVE,				/* set once data has started coming in */
};

typedef struct {
	tree_node tnode;
	char *user;
	char *node;
	char *href;
	unsigned int start;
	unsigned int transmit;
	unsigned int total;
	char *status;
	enum transfer_state state;
	struct bandwidth_meas bw;
} source;

typedef struct _transfer_t {
	tree_node tnode;
	gift_id id;
	char *filename;
	char *protocol;
	guint64 filesize, transferred;
	GTimer *last_time;
	char *hash;
	char *status;
	unsigned int elapsed;		/* Time elapsed between last updates */
	struct bandwidth_meas bw;

	unsigned paused:1;
	unsigned autodestroy:1;

	/* Download specific variables */
	gift_id search_id;			/* id receiving search results from giFT */
} transfer;

typedef struct {
	tree tnode;
	const char *sort_order;
	int update_ui;
	char **sort_methods;
	int sort_method;
} transfer_tree;

void source_cancel(transfer * t, source * s);
void source_touch(source *);

void transfers_init(void);

transfer *transfer_find(transfer_tree *, const char *hash, unsigned int size, gboolean deadness);
source *lookup_source(transfer * t, const char *user, const char *href);
void transfer_touch(transfer *);
void transfer_cancel(transfer *);
void transfer_forget(transfer *);
void transfer_reset(transfer *);
void transfer_suspend(transfer *);
int transfers_calculate(void);

extern int autodestroy_all;
extern tree_class_t transfer_class, source_class;
extern transfer_tree downloads, uploads;

int transfer_alive(transfer *);
char *transfer_change_sort_method(transfer_tree * active_tree, int dir);
char *source_change_sort_method(int dir);

#endif
