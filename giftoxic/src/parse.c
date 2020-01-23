/* giFToxic, a GTK2 based GUI for giFT
 * Copyright (C) 2002, 2003 giFToxic team (see AUTHORS)
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
 */

#include <gtk/gtk.h>
#include <libgift/libgift.h>
#include "config.h"
#include "common.h"
#include "main.h"
#include "transfer.h"
#include "utils.h"
#include "stats.h"
#include "event_ids.h"
#include "search.h"
#include "io.h"
#include "search_tab.h"

#define MAX_HITS    500
#define INTERFACE_GETDBL(p,k) g_ascii_strtod(interface_get((p), (k)), NULL)

extern Options		*options;
extern gchar		*search_status;
extern gint			running_searches;
gboolean            connected = FALSE; /* are we connected to remote networks? */

static void handle_attach(Interface *iface)
{
	stats_get(NULL);
    g_timeout_add(5000, (GSourceFunc) stats_get, NULL);
	
    transfer_get_sources_all(NULL);
    g_timeout_add(300000, (GSourceFunc) transfer_get_sources_all, NULL);
}

static SearchResult *get_searchresult_data(Interface *iface)
{
	SearchResult	*sr;

	sr = g_new0(SearchResult, 1);

	if (!(sr->url = g_strdup(interface_get(iface, "url")))) {
		g_free(sr);
		return NULL;
	} else
		url_decode(&sr->url);

	sr->user = g_strdup(interface_get(iface, "user"));
	sr->node = g_strdup(interface_get(iface, "node"));
	sr->availability = (INTERFACE_GETI(iface, "availability") > 0) ? TRUE : FALSE;
	sr->filesize = INTERFACE_GETLU(iface, "size");
	sr->mime = g_strdup(interface_get(iface, "mime"));
	sr->hash = g_strdup(interface_get(iface, "hash"));
	sr->filename = get_filename_from_path(interface_get(iface, "file"));
	sr->dir = get_dir_from_path(interface_get(iface, "file"));
	sr->bitrate = g_strdup(interface_get(iface, "meta/bitrate"));
	
	return sr;
}

static void handle_searchresult(Interface *iface)
{
    SearchResult	*sr;
    EventID			*id;
    static GSList	*sources;
    static gulong	search_hits;
	gulong			results;
	gchar			buf[128];

	id = event_id_lookup(atoi(iface->value));

	g_assert(id->type == EVENT_SEARCH || id->type == EVENT_SRC_SEARCH);
    
	if (search_hits >= MAX_HITS) {
		/* cancel search */
		snprintf(buf, sizeof(buf), "%i", id->num);
		gift_send(2, "SEARCH", buf, "action", "cancel");
	}

    if (!(sr = get_searchresult_data(iface))) { /* all search results have been sent */
		running_searches--;
		
		if (id->type == EVENT_SEARCH) {
			if (!running_searches) {
				results = get_search_results_num();
				if (results == 1)
					snprintf(buf, sizeof(buf), _("Search: got 1 result"));
				else
					snprintf(buf, sizeof(buf), _("Search: got %lu results"), results);

				statusbar_set(STATUSBAR_MISC, buf);
				gui_enable_stop(FALSE);
			}
		} else if (id->type == EVENT_SRC_SEARCH && sources) {
			transfer_add_sources(sources);
			g_slist_foreach(sources, free_searchresult, NULL);
			g_slist_free(sources);
			sources = NULL;
		}
	
		search_hits = 0;
		event_id_remove(id);

		return;
    }

    if (id->type == EVENT_SEARCH) {
		if (!add_searchresult(sr))
			search_hits++;
		free_searchresult(sr, NULL);
    } else if (id->type == EVENT_SRC_SEARCH)
		sources = g_slist_prepend(sources, sr);
}

static Transfer *get_transfer_data(Interface *iface, gboolean is_change)
{
    Transfer	*transfer;
	gchar		*state;

    transfer = g_new0(Transfer, 1);
    
	transfer->hash = g_strdup(interface_get(iface, "hash"));
	transfer->transmitted = INTERFACE_GETLU(iface, "transmit");
	transfer->size = INTERFACE_GETLU(iface, "size");
	transfer->filename = g_strdup(interface_get(iface, "file"));

	state = interface_get(iface, "state");

	if (!strcasecmp(state, "paused"))
		transfer->status = STATUS_PAUSED;
	else if (!strcasecmp(state, "active"))
		transfer->status = STATUS_ACTIVE;
	else if (!strcasecmp(state, "completed"))
		transfer->status = STATUS_COMPLETE;

	if (is_change) {
		transfer->throughput = INTERFACE_GETLU(iface, "throughput");
		transfer->elapsed = INTERFACE_GETLU(iface, "elapsed");
	}

    return transfer;
}

static void get_chunk_data(Interface *iface, InterfaceNode *node, Chunk **chunk)
{
	if (!strcasecmp(node->key, "user"))
		(*chunk)->user = g_strdup(node->value);
	else if (!strcasecmp(node->key, "url")) {
		(*chunk)->url = g_strdup(node->value);
		url_decode(&(*chunk)->url);
	} else if (!strcasecmp(node->key, "status"))
		(*chunk)->status = g_strdup(node->value);
	else if (!strcasecmp(node->key, "start"))
		(*chunk)->start = atol(node->value);
	else if (!strcasecmp(node->key, "transmit"))
		(*chunk)->transmitted = atol(node->value);
	else if (!strcasecmp(node->key, "total"))
		(*chunk)->size = atol(node->value);
}

static void traverse_tree_sources(Interface *iface, InterfaceNode *node, EventID *id)
{
	Chunk	*chunk;
	
	if (!strcasecmp(node->key, "SOURCE")) {
		chunk = g_new0(Chunk, 1);
		chunk->id = id;
	
		interface_foreach_ex(iface, node, (InterfaceForeach) get_chunk_data, &chunk);

		chunk_add(chunk);
		free_chunk(chunk);
	}
}

static void handle_transfer(Interface *iface, EventType type, gboolean is_change_ev)
{
    EventID		*id;
    Transfer	*transfer;
    gint		session_id;

	session_id = atoi(iface->value);

    /* transfer ID's are chosen by giFT: */
    if (!(id = event_id_lookup(session_id)))
		id = event_id_add(session_id, type);
    
    transfer = get_transfer_data(iface, is_change_ev);
    transfer->id = id;

    transfer_add(transfer);
    free_transfer(transfer);

	interface_foreach(iface, NULL, (InterfaceForeach) traverse_tree_sources, id);
}

static void handle_transfer_del(Interface *iface, EventType type)
{
	TransferStatus	status;
	EventID			*id = event_id_lookup(atoi(iface->value));

	status = get_transfer_status(id);
	
	if (status == STATUS_COMPLETE) {
		if (options->autoclean & AUTO_CLEAN_COMPLETED)
			transfer_remove(id->type, 1, id->num);
	} else if (options->autoclean & AUTO_CLEAN_CANCELLED)
		transfer_remove(id->type, 1, id->num);
	else
		set_transfer_status(id, STATUS_CANCELLED);

	set_transfer_id(id, -1);
	event_id_remove(id);
}

static void handle_share(Interface *iface)
{
	gchar	*val = interface_get(iface, "action");

	if (!strcasecmp(val, "hide") && options->sharing)
		set_sharing_status(FALSE);
	else if (!strcasecmp(val, "show") && !options->sharing)
		set_sharing_status(TRUE);
	else if (!strcasecmp(val, "sync")) {
		if (!strcasecmp(interface_get(iface, "STATUS"), "done"))
			statusbar_set(STATUSBAR_MISC, _("Shares synced!"));
		else
			statusbar_set(STATUSBAR_MISC, _("Syncing shares..."));
	}
}

static void get_stats_data(Interface *iface, InterfaceNode *node, Stats **stats)
{
	if (!strcasecmp(node->key, "users"))
		(*stats)->users += strtoul(node->value, NULL, 10);
	else if (!strcasecmp(node->key, "files"))
		(*stats)->files_remote += strtoul(node->value, NULL, 10);
	else if (!strcasecmp(node->key, "size"))
		(*stats)->size_remote += g_strtod(node->value, NULL);
}

static void traverse_tree_stats(Interface *iface, InterfaceNode *node, Stats **stats)
{
	if (strcasecmp(node->key, "gift")) {
		(*stats)->networks = g_slist_prepend((*stats)->networks, node->key);
		interface_foreach(iface, node->key, (InterfaceForeach) get_stats_data, stats);
	}
}

static gboolean is_connected(gchar *network, Interface *iface)
{
	gchar  key[128];

	snprintf(key, sizeof(key), "%s/users", network);
	return INTERFACE_GETLU(iface, key) > 0;
}

static gchar *get_connected_networks(Interface *iface, GSList *networks)
{
	gchar *buf = g_strdup("");
	gint  connections = 0;
	
	while (networks) {
		if (is_connected(networks->data, iface))
			buf = strconcat(buf, (connections++ ? ", " : " "), networks->data, NULL);

		networks = networks->next;
	}
	
	if (!connections)
		buf = strconcat(buf, _("Not Connected"), NULL);
	
	return buf;
}


static void handle_stats(Interface *iface)
{
	Stats			*stats;
    gchar			uploads[128];
    gchar			downloads[128];
    gchar			buf[128];
	gboolean 		connection_changes = FALSE;
	static gchar	*old_connections = NULL;
	
	stats = g_new0(Stats, 1);
	
    /* local stuff (GIFT) */
	stats->files_local = INTERFACE_GETLU(iface, "gift/files");
	stats->size_local = INTERFACE_GETDBL(iface, "gift/size");
	
	/* get remove stuff data */
	interface_foreach(iface, NULL, (InterfaceForeach) traverse_tree_stats, &stats);

	get_transfer_num(&stats->downloads, &stats->uploads);
	stats->connected_networks = get_connected_networks(iface, stats->networks);
	
	if (old_connections) {
		connection_changes = strcmp(stats->connected_networks, old_connections);
		g_free(old_connections);
	} else
		connection_changes = TRUE;
	
	old_connections = g_strdup(stats->connected_networks);
	
	/* update the labels and the statusbar */
    stats_update(stats);

	strcpy(buf, (stats->downloads == 1)?_("1 active download"):_("%i active downloads"));
	sprintf(downloads, buf, stats->downloads);
	strcpy(buf, (stats->uploads== 1)?_("1 active upload"):_("%i active uploads"));
	sprintf(uploads, buf, stats->uploads);
	strcpy(buf,"");
	strcat(buf, downloads);
	strcat(buf, ", ");
	strcat(buf, uploads);
	
	statusbar_set(STATUSBAR_TRANSFERS, buf);

	if (stats->networks && connection_changes) {
		connected = TRUE;
		statusbar_set(STATUSBAR_MISC, _("Connected to: %s"), stats->connected_networks);
	}

	while (stats->networks)
		stats->networks = g_slist_remove(stats->networks, stats->networks->data);

	g_slist_free(stats->networks);
	g_free(stats->connected_networks);
	g_free(stats);
}

void parse_daemon_msg(gchar *input)
{
	Interface	*i = NULL;
	gchar		*c;
	
	if (!(i = interface_unserialize(input, strlen(input))))
		return;

	c = i->command;

	if (!strcasecmp(c, "attach"))
		handle_attach(i);
	else if (!strcasecmp(c, "share"))
		handle_share(i);
	else if (!strcasecmp(c, "stats"))
		handle_stats(i);
	else if (!strcasecmp(c, "item"))
		handle_searchresult(i);
	else if (!strcasecmp(c, "adddownload"))
		handle_transfer(i, EVENT_DOWNLOAD, FALSE);
	else if (!strcasecmp(c, "chgdownload"))
		handle_transfer(i, EVENT_DOWNLOAD, TRUE);
	else if (!strcasecmp(c, "addupload"))
		handle_transfer(i, EVENT_UPLOAD, FALSE);
	else if (!strcasecmp(c, "chgupload"))
		handle_transfer(i, EVENT_UPLOAD, TRUE);
	else if (!strcasecmp(c, "deldownload"))
		handle_transfer_del(i, EVENT_DOWNLOAD);
	else if (!strcasecmp(c, "delupload"))
		handle_transfer_del(i, EVENT_UPLOAD);

	interface_free(i);
}
