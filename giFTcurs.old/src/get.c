/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 G�ran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian H�ggstr�m <chm@c00.info>
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
 * $Id: get.c,v 1.195 2003/05/11 00:56:53 chnix Exp $
 */
#include "giftcurs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "parse.h"
#include "get.h"
#include "transfer.h"
#include "screen.h"
#include "protocol.h"
#include "xcommon.h"
#include "search.h"
#include "settings.h"

static void add_source(const char *user, const char *hash, unsigned int size, const char *href,
					   const char *filename);
static void download_incoming_source_handler(ntree *, transfer *);
static int download_search_more(transfer *);

int download_search(transfer * t)
{
	if (!t->hash)
		return 0;

	if (t->search_id) {
		g_message(_("A source search for this file is already in progress."));
		return 0;
	}

	if (!t->id) {
		int pos = tree_hold(t);

		transfer_reset(t);
		tree_release(t, pos);
	}

	return download_search_more(t);
}

/* Called to start a new download. Pass a subhit as second argument to do
 * single-source download, a NULL in second argument for multi-source d/l
 */
/* Note. this function doesn't modify any tables, they are modified later
 * when we get response from the daemon */
int download_hit(hit * info, subhit * single)
{
	transfer *t = NULL;

	if (single)
		info = single->tnode.parent;
	if (!info->filesize) {
		/* Downloading this file causes some really weird behaviour.
		   Btw, don't mark for translation :) */
		g_message("I'm sorry Dave, I'm afraid I can't do that.");
		return 0;
	}

	/* Check if we're already downloading this one. */
	if (info->hash)
		t = transfer_find(&downloads, info->hash, info->filesize, 0);

	if (single) {
		if (t && lookup_source(t, single->user, single->href)) {
			g_message(_("Already downloading from this source"));
			return 0;
		}
		add_source(single->user, info->hash, info->filesize, single->href, info->filename);
		g_message(_("Downloading from a single source"));
	} else {
		int i, added = 0;
		list *sources;

		if (t && !transfer_alive(t)) {
			download_search(t);
			g_message(_("Revived '%s'."), info->filename);
			return 0;
		}

		sources = tree_children(info);

		if (sources->num == 0) {
			g_message(_("I have no sources for '%s'."), info->filename);
			return 0;
		}

		/* Add the sources we have so far... */
		for (i = 0; i < sources->num; i++) {
			subhit *sh = list_index(sources, i);

			if (t && lookup_source(t, sh->user, sh->href))
				continue;
			add_source(sh->user, info->hash, info->filesize, sh->href, info->filename);
			added++;
		}
		if (!added) {
			g_message(_("Already downloading '%s'."), info->filename);
			return 0;
		}
		g_message(_("Downloading..."));
	}

	return 0;
}

static void add_source(const char *user, const char *hash, unsigned int size, const char *href,
					   const char *filename)
{
	ntree *packet = NULL;

	interface_append(&packet, "ADDSOURCE", NULL);
	interface_append(&packet, "user", user);
	if (hash)
		interface_append(&packet, "hash", hash);
	interface_append_int(&packet, "size", size);
	interface_append(&packet, "url", href);
	interface_append(&packet, "save", filename);
	gift_write(&packet);
}

static void download_incoming_source_handler(ntree * data, transfer * t)
{
	const char *href, *user, *hash;
	unsigned int filesize;

	if (interface_isempty(data)) {
		/* end of search */
		gift_release_id(t->search_id);
		t->search_id = 0;
		g_message(_("Source search complete for '%s'."), t->filename);
		return;
	}

	href = interface_lookup(data, "url");
	user = interface_lookup(data, "user");
	hash = interface_lookup(data, "hash");
	filesize = my_atoi(interface_lookup(data, "size"));

	if (!href || !filesize || !hash || !user) {
		DEBUG("Source not usable.");
		return;
	}

	if (user_isignored(user))
		return;

	/* Check if the hash really is what we searched for. */
	if (strcmp(hash, t->hash)) {
		DEBUG("Source hash differ from search query.");
		return;
	}
	add_source(user, hash, filesize, href, t->filename);
}

static int download_search_more(transfer * t)
{
	ntree *packet = NULL;

	t->search_id = gift_new_id();

	interface_append_int(&packet, "LOCATE", t->search_id);
	interface_append(&packet, "query", t->hash);
	if (t->protocol)
		interface_append(&packet, "protocol", t->protocol);
	if (gift_write(&packet) < 0) {
		t->search_id = 0;
		return -1;
	}
	g_message(_("Started a source search for '%s'."), t->filename);
	gift_register_id(t->search_id, (EventCallback) download_incoming_source_handler, t);

	return 0;
}
