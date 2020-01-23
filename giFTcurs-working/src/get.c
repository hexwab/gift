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
 * $Id: get.c,v 1.182 2002/11/28 19:52:23 chnix Exp $
 */
#include "giftcurs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "get.h"
#include "transfer.h"
#include "screen.h"
#include "poll.h"
#include "protocol.h"
#include "xcommon.h"
#include "search.h"
#include "settings.h"

static void add_source(char *user, char *hash, unsigned int size, char *href, char *filename);
static void md5sum_append(char *md5file, char *hash, char *filename);
static void download_incoming_source_handler(ntree *, transfer *);
static int download_search_more(transfer *);

int download_search(transfer * t)
{
	if (!t->hash)
		return 0;

	if (t->search_id) {
		message(_("A source search for this file is already in progress."));
		return 0;
	}

	if (!t->id)
		transfer_reset(t);

	return download_search_more(t);
}

/* Called to start a new download. Pass a subhit as second argument to do
 * single-source download, a NULL in second argument for multi-source d/l
 */
int download_hit(hit * info, subhit * single)
{
	transfer *t = NULL;
	char *md5file = get_config("set", "md5file", NULL);

	if (single)
		info = single->parent;
	if (!info->filesize) {
		/* Downloading this file causes some really weird behaviour.
		   Btw, don't mark for translation :) */
		message("I'm sorry Dave, I'm afraid I can't do that.");
		return 0;
	}

	/* Check if we're already downloading this one. */
	if (info->hash)
		t = transfer_find(&downloads, info->hash, info->filesize, 0);

	if (single) {
		if (t && lookup_source(t, single->user, single->href)) {
			message(_("Already downloading from this source"));
			return 0;
		}
		add_source(single->user, info->hash, info->filesize, single->href, info->filename);
		message(_("Downloading from a single source"));
	} else {
		int i, added = 0;

		if (t && !transfer_alive(t)) {
			download_search(t);
			message(_("Revived '%s'."), info->filename);
			return 0;
		}

		/* Add the sources we have so far... */
		for (i = 0; i < info->sources.num; i++) {
			subhit *sh = list_index(&info->sources, i);

			if (t && lookup_source(t, sh->user, sh->href))
				continue;
			add_source(sh->user, info->hash, info->filesize, sh->href, info->filename);
			added++;
		}
		if (!added) {
			message(_("Already downloading '%s'."), info->filename);
			return 0;
		}
		message(_("Downloading..."));
	}

	/* Append the hash to a file */
	if (md5file && !t && info->hash)
		md5sum_append(md5file, info->hash, info->filename);

	return 0;
}

static void add_source(char *user, char *hash, unsigned int size, char *href, char *filename)
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
	char *href, *user, *hash;
	unsigned int filesize;

	if (!data || !(href = interface_lookup(data, "url"))) {
		/* end of search */
		gift_release_id(t->search_id);
		t->search_id = 0;
		message(_("Source search complete for '%s'."), t->filename);
		return;
	}

	user = interface_lookup(data, "user");
	hash = interface_lookup(data, "hash");
	filesize = my_atoi(interface_lookup(data, "size"));

	if (!filesize || !hash || !user) {
		DEBUG("Source not usable.");
		return;
	}

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
	message(_("Started a source search for '%s'."), t->filename);
	gift_register_id(t->search_id, (EventCallback) download_incoming_source_handler, t);

	return 0;
}


/* This allows people to do 'md5sum -c MD5SUMS' or something like that. */
static void md5sum_append(char *md5file, char *hash, char *filename)
{
	FILE *f;
	char *fname;

	if (md5file[0] == '/')
		/* Path is absolute */
		fname = strdup(md5file);
	else {
		/* Path is relative to completed/ */
		if (!download_dir) {
			DEBUG("Didn't have the path to the completed directory.");
			return;
		}
		asprintf(&fname, "%s/%s", download_dir, md5file);
	}

	f = fopen(fname, "a");

	free(fname);

	if (!f) {
		ERROR("Couldn't append md5 to '%s'", md5file);
		return;
	}

	fprintf(f, "%s  %s\n", hash, filename);

	fclose(f);
}
