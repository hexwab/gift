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
 * $Id: transfer.h,v 1.37 2002/11/23 16:23:56 chnix Exp $
 */
#ifndef _TRANSFER_H
#define _TRANSFER_H

#include "parse.h"
#include "tree.h"
#include "gift.h"
#include "poll.h"

enum {
	TRANSFER_UPLOAD,
	TRANSFER_DOWNLOAD
};

struct _transfer_t;

typedef struct {
	rendering *methods;			/* How to display data. MUST be first item */
	char *user;
	char *node;
	char *href;
	unsigned int start;
	unsigned int transmit;
	unsigned int total;
	char *status;
	char *pretty;
	struct _transfer_t *parent;
} source;

typedef struct _transfer_t {
	rendering *methods;			/* How to display data. MUST be first item */
	gift_id id;
	char *filename;
	char *protocol;
	unsigned int filesize, transferred, bandwidth;
	unsigned int last_time;
	char *pretty;
	char *hash;
	list sourcen;
	char *status;

	unsigned paused:1;
	unsigned autodestroy:1;
	unsigned type:1;
	unsigned expanded:1;

	/* Download specific variables */
	gift_id search_id;			/* id receiving search results from giFT */
} transfer;

void source_cancel(transfer * t, source * s);
void source_destroy(source *);
void source_touch(source *);

void transfers_init();

transfer *transfer_find(tree *, char *hash, unsigned int size, int deadness);
source *lookup_source(transfer * t, const char *user, const char *href);
void transfer_destroy(transfer *);
void transfer_touch(transfer *);
void transfer_detach(transfer *);
void transfer_cancel(transfer *);
void transfer_forget(transfer *);
void transfer_reset(transfer *);
void transfer_suspend(transfer *);
int transfers_calculate(void);

extern int autodestroy_all;
extern rendering transfer_methods, source_methods;
extern tree downloads, uploads;

int transfer_alive(transfer *);

#endif
