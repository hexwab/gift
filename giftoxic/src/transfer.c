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
#include <string.h>
#include <stdio.h>
#include "common.h"
#include "io.h"
#include "event_ids.h"
#include "utils.h"
#include "search.h"

extern GtkTreeStore	*stores[STORE_NUM];

/* reset the speed columns; called periodically (every 3 seconds)
 * if the row isn't updated bt giFT, reset the transfer speed
 */
gboolean reset_speed_cols(gpointer data)
{
	GtkTreeModel	*tree_model;
	GtkTreeIter		iter;
	gint			i;
	gint			updated;
	gboolean		tree_not_empty;
	
	for (i = 0; i < 2; i++) {
		if (!stores[i])
			continue;
		
		tree_model = GTK_TREE_MODEL(stores[i]);
		tree_not_empty = gtk_tree_model_get_iter_first(tree_model, &iter);

		while (tree_not_empty) {
	        gtk_tree_model_get(tree_model, &iter, TUPDATED, &updated, -1);
			
	        if (!updated) 
	        	gtk_tree_store_set(stores[i], &iter, TSPEED, "0.0 kb/s", TETA, "---", -1);

	        // mark as Not Updated for the next iteration
			gtk_tree_store_set(stores[i], &iter, TUPDATED, 0, -1);
	
			tree_not_empty = gtk_tree_model_iter_next(tree_model, &iter);
		}
	}

	return TRUE;
}

/* loop over all children of @iter and compare the value in column @col with @data
 * if they match, return TRUE and set @iter to the the child with the matching data
 * if no matching entry is found, return FALSE
 */
gboolean replace_child(GtkTreeModel *tree_model, GtkTreeIter *iter, gint col, gchar *data)
{
    GtkTreeIter		iter_child;
    gchar			*c;
    gboolean	    foo;

    foo = gtk_tree_model_iter_children(tree_model, &iter_child, iter);

    while (foo) {
		gtk_tree_model_get(tree_model, &iter_child, col, &c, -1);

		if (!strcmp(data, c)) {
			*iter = iter_child;
			g_free(c);
			return TRUE;
		}

		g_free(c);
		foo = gtk_tree_model_iter_next(tree_model, &iter_child);
    }

    return FALSE;
}

gboolean chunk_add(Chunk *chunk)
{
    GtkTreeModel	    *tree_model;
    GtkTreeIter		    iter;
    GtkTreeIter		    iter_bak;
    gchar				*user;
	gchar				*user_unique;
	gchar				*tmp;
    gchar				*visible_size;
    gint				id_in_list;
    gfloat				progress = 0;
    gboolean		    tree_not_empty;

    g_assert(chunk);
    g_assert(chunk->id->type == EVENT_UPLOAD || chunk->id->type == EVENT_DOWNLOAD);

    /* we convert the "user" string to utf8 because it may contain locale-specific chars
     * pango can't deal with
     */
    user_unique = locale_to_utf8(chunk->user);
	tmp = get_user_from_ident(chunk->user);
	user = locale_to_utf8(tmp);
	g_free(tmp);
    
    visible_size = g_strdup("0 KB");
    
    if (chunk->size)
		progress = (gfloat) chunk->transmitted / chunk->size * 100;
    
    tree_model = GTK_TREE_MODEL(stores[chunk->id->type]);
    tree_not_empty = gtk_tree_model_get_iter_first(tree_model, &iter);

    g_assert(tree_not_empty);
    
    iter_bak = iter; 

    while (tree_not_empty) {
		iter = iter_bak;
    
		gtk_tree_model_get(tree_model, &iter, TID, &id_in_list, -1);

		if (id_in_list == chunk->id->num) {
			if (replace_child(tree_model, &iter, TUSER_UNIQUE, user_unique)) {
				/* we don't get THROUGHPUT and ELAPSED information for chunks/sources, so we can't compute speed
				if (strcmp(chunk->status, "Complete"))
				{
					gtk_tree_model_get(tree_model, &iter, TTRANS, &trans_old, -1);
					kbs = (gfloat) (chunk->transmitted - trans_old) / 1024;
					g_free(speed);
					speed = g_strdup_printf("%'.1f kb/s", kbs);
				}*/
				
				gtk_tree_store_set(stores[chunk->id->type], &iter, TSTATUS, chunk->status, TPROGRESS, progress, TTRANS, chunk->transmitted, TUPDATED, 1, -1);
			} else {
				g_free(visible_size);
				visible_size = size_human(chunk->size);

				gtk_tree_store_append(stores[chunk->id->type], &iter, &iter_bak); /* iter = child; iter_bak = parent */
				gtk_tree_store_set(stores[chunk->id->type], &iter, TUSER, user, TUSER_UNIQUE, user_unique, TSTATUS, chunk->status, TPROGRESS, progress, TSIZE, chunk->size, TSIZEHUMAN, visible_size, TTRANS, chunk->transmitted, TID, id_in_list, TUPDATED, 1,  -1);
			}
		}
	
		tree_not_empty = gtk_tree_model_iter_next(tree_model, &iter_bak);
    }

    g_free(user);
	g_free(user_unique);
    g_free(visible_size);

    return TRUE;
}

gboolean transfer_add(Transfer *transfer)
{
    gchar			*status[STATUS_NUM] = {"Active", "Paused", "Complete", "Cancelled"};
    GtkTreeModel	*tree_model;
    GtkTreeIter		iter;
    gchar		    *filename;
    gchar		    speed[32];
    gchar			*visible_size;
	gchar			*eta; /* remaining time */
    gint		    id_in_list;
    gfloat		    progress = 0;
    gfloat		    kbs;
    gboolean		tree_not_empty;
    gboolean		added_element = FALSE;
	gulong			tracked_time = 0;
	gulong			tracked_size = 0;

    g_assert(transfer);
    g_assert(transfer->id->type == EVENT_UPLOAD || transfer->id->type == EVENT_DOWNLOAD);
        
    /* the filename may contain locale-specific chars, so we convert it to utf8 */
    filename = locale_to_utf8(transfer->filename);

	snprintf(speed, sizeof(speed), "0.0 kb/s");
	eta = g_strdup("---");
    
    if (transfer->size)
		progress = (gfloat) transfer->transmitted / transfer->size * 100;
    
    tree_model = GTK_TREE_MODEL(stores[transfer->id->type]);
    tree_not_empty = gtk_tree_model_get_iter_first(tree_model, &iter);

    while (tree_not_empty) {
        gtk_tree_model_get(tree_model, &iter, TID, &id_in_list, -1);

		if (id_in_list == transfer->id->num) {
			/* there's an existing item with the same transfer id, so we replace the data in that entry */
			
			if (transfer->elapsed) {
				gtk_tree_model_get(tree_model, &iter, TTRACKED_TIME, &tracked_time, TTRACKED_SIZE, &tracked_size, -1);

				tracked_time += transfer->elapsed;
				kbs = (gfloat) tracked_size / tracked_time;
				tracked_size += transfer->throughput;
				
				gtk_tree_store_set(stores[transfer->id->type], &iter,
					TTRACKED_TIME, tracked_time,
					TTRACKED_SIZE, tracked_size, -1);

				snprintf(speed, sizeof(speed), "%.1f kb/s", kbs);

				if (kbs) {
					g_free(eta);
					eta = time_human((gfloat) (transfer->size - transfer->transmitted) / 1024 / kbs);
				}
			}
	    
			gtk_tree_store_set(stores[transfer->id->type], &iter,
					TSTATUS, status[transfer->status],
					TPROGRESS, progress,
					TTRANS, transfer->transmitted,
					TSPEED, speed,
					TETA, eta,
					TSTATUS_NUM, transfer->status, 
					TUPDATED, 1, -1);
			
			added_element = TRUE;
			break;
		}
	    
		tree_not_empty = gtk_tree_model_iter_next(tree_model, &iter);
    }

    if (!added_element) {
    	visible_size = size_human(transfer->size);

        /* create a new item */
        gtk_tree_store_append(stores[transfer->id->type], &iter, NULL);
        gtk_tree_store_set(stores[transfer->id->type], &iter,
				TNAME, filename,
				TSTATUS, status[transfer->status],
				TPROGRESS, progress,
				TSIZE, transfer->size,
				TSIZEHUMAN, visible_size,
				TSPEED, speed,
				TTRACKED_TIME, 0,
				TTRACKED_SIZE, 0,
				TETA, eta,
				TTRANS,	transfer->transmitted,
				THASH, transfer->hash,
				TSTATUS_NUM, transfer->status,
				TID, transfer->id->num,
				TUPDATED, 1,  -1);
    	g_free(visible_size);
    }
   
    g_free(filename);
	g_free(eta);

    return FALSE;
}

void transfer_get_sources(GtkMenuItem *menu_item, Moo *data)
{
    GtkTreeModel	    *tree_model;
	GtkTreeIter			iter;
    gchar				*hash = NULL;

    tree_model = GTK_TREE_MODEL(stores[data->store]);
	gtk_tree_model_get_iter(tree_model, &iter, data->path);
    gtk_tree_model_get(tree_model, &iter, THASH, &hash, -1);

    start_source_search(hash);
    g_free(hash);

    gtk_tree_path_free(data->path);
	g_free(data);

    return;
}

/* looks for sources for all active (non-paused) downloads */
gboolean transfer_get_sources_all(gpointer data)
{
    GtkTreeModel	    *tree_model;
    GtkTreeIter		    iter;
    TransferStatus	    status;
    gchar				*hash = NULL;
    gboolean		    entries_left;

    tree_model = GTK_TREE_MODEL(stores[STORE_DOWNLOAD]);
    
    entries_left = gtk_tree_model_get_iter_first(tree_model, &iter);

    while (entries_left) {
		gtk_tree_model_get(tree_model, &iter, THASH, &hash, TSTATUS_NUM, &status, -1);

		if (status == STATUS_ACTIVE)
			start_source_search(hash);

		g_free(hash);
	    
		entries_left = gtk_tree_model_iter_next(tree_model, &iter);
    }

    return TRUE;
}

gboolean transfer_add_sources(GSList *sources)
{
    SearchResult	*sr;
    GSList			*list;
	gchar			buf[16];
    
	for (list = sources; list; list = list->next) {
		sr = (SearchResult *) list->data;
	
		escape(&sr->user);
		escape(&sr->url);
		escape(&sr->filename);
	
		snprintf(buf, sizeof(buf), "%ld", sr->filesize);
		gift_send(6, "ADDSOURCE", NULL, "user", sr->user, "hash", sr->hash, "size", buf, "url", sr->url, "save", sr->filename);
    }
    
	return FALSE;
}

void file_download(GtkMenuItem *menu_item, GtkTreePath *path)
{
    GtkTreeModel	*tree_model;
    GtkTreeIter		iter;
    gchar		    *filename;
    gchar		    *hash;
    gchar		    *user;
    gchar		    *url;
	gchar			buf[16];
    gulong		    size;

    tree_model = GTK_TREE_MODEL(stores[STORE_SEARCH]);
    
    gtk_tree_model_get_iter(tree_model, &iter, path);
    gtk_tree_model_get(tree_model, &iter, SNAME, &filename, SSIZE, &size, SHASH, &hash, SUSER, &user, SURL, &url, -1);

    escape(&user);
    escape(&url);
    escape(&filename);
    
    start_source_search(hash);
    
	snprintf(buf, sizeof(buf), "%ld", size);
	gift_send(6, "ADDSOURCE", NULL,  "user", user, "hash", hash, "size", buf, "url", url, "save", filename);
   
    g_free(filename);
    g_free(hash);
    g_free(url);
    g_free(user);
    gtk_tree_path_free(path);

    return;
}

void transfer_pause(GtkMenuItem *menu_item, Moo *data)
{
	gchar buf[16];
	
	snprintf(buf, sizeof(buf), "%i", data->id);
    gift_send(2, "TRANSFER", buf, "action", "pause");

    gtk_tree_path_free(data->path);
	g_free(data);

    return;
}

void transfer_resume(GtkMenuItem *menu_item, Moo *data)
{
	gchar buf[16];
	
	snprintf(buf, sizeof(buf), "%i", data->id);
    gift_send(2, "TRANSFER", buf, "action", "unpause");
    
    gtk_tree_path_free(data->path);
	g_free(data);

    return;
}

/* removes transfers that are stored in @store.
 * @count gives the number of ids to search for,
 * followed by the list of ids
 */
void transfer_remove(TreeStore store, gint count, ...)
{
	GtkTreeModel	*tree_model;
	GtkTreeIter		iter;
	GSList			*id_list = NULL;
	GSList			*list;
	va_list			args;
	gboolean		entries_left;
	gboolean		iter_moved;
	gint			id;
	gint			id_in_tree;
	gint			i;
	
	va_start(args, count);

	for (i = 0; i < count; i++) {
		id = va_arg(args, gint);
		id_list = g_slist_prepend(id_list, GINT_TO_POINTER(id));
	}

	va_end(args);

	if (!id_list)
		return;

	tree_model = GTK_TREE_MODEL(stores[store]);
	entries_left = gtk_tree_model_get_iter_first(tree_model, &iter);

	while (entries_left) {
		iter_moved = FALSE;
		gtk_tree_model_get(tree_model, &iter, TID, &id_in_tree, -1);

		for (list = id_list; list; list = list->next) {
			id = GPOINTER_TO_INT(list->data);
			
			if (id == id_in_tree) {
#if (GTK_MINOR_VERSION < 2)
				gtk_tree_store_remove(stores[store], &iter);
				return;
#else
				iter_moved = gtk_tree_store_remove(stores[store], &iter);
#endif
				/* iter was pointing at the last row, so we are done
				 * if iter_moved is TRUE, iter was already set to the next row
				 * so we don't need to set it below
				 */
				
				if (!iter_moved)
					return;
			}
		}

		if (!iter_moved)
			entries_left = gtk_tree_model_iter_next(tree_model, &iter);
	}

	return;
}

void transfer_remove_with_path(GtkMenuItem *menu_item, Moo *data)
{
	GtkTreeModel		*tree_model;
	GtkTreeIter			iter;
	
	tree_model = GTK_TREE_MODEL(stores[data->store]);
    
    gtk_tree_model_get_iter(tree_model, &iter, data->path);
	gtk_tree_store_remove(stores[data->store], &iter);

    gtk_tree_path_free(data->path);
	g_free(data);
	
	return;
}

/* removes all transfers with transfer->id->num == -1,
 * i.e. completed or cancelled transfers
 */
void transfer_remove_all(GtkMenuItem *menu_item, Moo *data)
{
	transfer_remove(data->store, 1, -1);
    gtk_tree_path_free(data->path);
	g_free(data);

	return;
}

void transfer_cancel(GtkMenuItem *menu_item, Moo *data)
{
	gchar buf[16];
	
	snprintf(buf, sizeof(buf), "%i", data->id);
    gift_send(2, "TRANSFER", buf, "action", "cancel");

    gtk_tree_path_free(data->path);
	g_free(data);

    return;
}

TransferStatus get_transfer_status(EventID *id)
{
	TransferStatus	status;
	GtkTreeModel	*tree_model;
	GtkTreeIter		iter;
	gint			id_in_list;
	gboolean		entries_left;

	tree_model = GTK_TREE_MODEL(stores[id->type]);

	entries_left = gtk_tree_model_get_iter_first(tree_model, &iter);

    while (entries_left) {
		gtk_tree_model_get(tree_model, &iter, TID, &id_in_list, TSTATUS_NUM, &status, -1);

		if (id_in_list == id->num)
			return status;
	    
		entries_left = gtk_tree_model_iter_next(tree_model, &iter);
    }

	return -1;
}

gboolean set_transfer_status(EventID *id, TransferStatus status)
{
    gchar			*status_str[STATUS_NUM] = {"Active", "Paused", "Complete", "Cancelled"};
	GtkTreeModel	*tree_model;
	GtkTreeIter		iter;
	gboolean		entries_left;
	gint			id_in_list;
	
	tree_model = GTK_TREE_MODEL(stores[id->type]);
	
	entries_left = gtk_tree_model_get_iter_first(tree_model, &iter);

	while (entries_left) {
		gtk_tree_model_get(tree_model, &iter, TID, &id_in_list, -1);

		if (id_in_list == id->num) {
			gtk_tree_store_set(stores[id->type], &iter, TSTATUS, status_str[status], TSTATUS_NUM, status, -1);
			return FALSE;
		}

		entries_left = gtk_tree_model_iter_next(tree_model, &iter);
	}

	return TRUE;
}

gboolean set_transfer_id(EventID *id, gint new_id)
{
	GtkTreeModel	*tree_model;
	GtkTreeIter		iter;
	gboolean		entries_left;
	gint			id_in_list;
	
	tree_model = GTK_TREE_MODEL(stores[id->type]);
	
	entries_left = gtk_tree_model_get_iter_first(tree_model, &iter);

	while (entries_left) {
		gtk_tree_model_get(tree_model, &iter, TID, &id_in_list, -1);

		if (id_in_list == id->num) {
			gtk_tree_store_set(stores[id->type], &iter, TID, new_id, -1);
			return FALSE;
		}

		entries_left = gtk_tree_model_iter_next(tree_model, &iter);
	}

	return TRUE;
}


/* fills *uploads and *downloads with the number of active uploads/downloads */
gboolean get_transfer_num(gint *downloads, gint *uploads)
{
	TransferStatus	status;
	GtkTreeModel	*tree_model;
	GtkTreeIter		iter;
	gint			i;
	gint			transfers[2] = {0, 0};
	gboolean		entries_left;
	
	for (i = 0; i < 2; i++) {
		tree_model = GTK_TREE_MODEL(stores[i]);
		entries_left = gtk_tree_model_get_iter_first(tree_model, &iter);

		while (entries_left) {
			gtk_tree_model_get(tree_model, &iter, TSTATUS_NUM, &status, -1);

			if (status != STATUS_COMPLETE && status != STATUS_CANCELLED)
				transfers[i]++;

			entries_left = gtk_tree_model_iter_next(tree_model, &iter);
		}
	}

	*downloads = transfers[0];
	*uploads = transfers[1];

	return FALSE;
}
