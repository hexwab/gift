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
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "common.h"
#include "utils.h"
#include "io.h"
#include "event_ids.h"
#include "stats.h"

extern gint running_searches;

void search_cancel(gpointer item, gpointer data)
{
	EventID	*id = (EventID *) item;
	gchar	buf[8];
	
	g_assert(id);

	if (id->type == EVENT_SEARCH) {
		snprintf(buf, sizeof(buf), "%i", id->num);
		gift_send(2, "SEARCH", buf, "action", "cancel");
	}
}

void stop_search()
{
	event_id_foreach(search_cancel); /* cancel pending searches */
	statusbar_set(STATUSBAR_MISC, _("Search: stopped"));
}

static gchar *build_final_search_string(gchar *include, gchar *exclude, SearchMode mode)
{
	Interface	*iface = NULL;
	String		*buf = NULL;
    gchar		*realms[5] = {"audio", "video", "image", "text", "application"};
    gchar		*result = NULL;
	gchar		id[8];

    if (include && strlen(include)) {
		snprintf(id, sizeof(id), "%i", event_id_get_new(EVENT_SEARCH));
		
		switch (mode) {
			case SEARCH_MODE_USER:
				iface = interface_new("BROWSE", id);
				break;
			case SEARCH_MODE_HASH:
				iface = interface_new("LOCATE", id);
				break;
			default:
				iface = interface_new("SEARCH", id);
				break;
		}

		interface_put(iface, "query", include);

		if (exclude && strlen(exclude))
			interface_put(iface, "exclude", exclude);
    } else
		return NULL;
	
    if (mode != SEARCH_MODE_ALL && mode != SEARCH_MODE_HASH && mode != SEARCH_MODE_USER)
		interface_put(iface, "realm", realms[mode - 1]);

	buf = interface_serialize(iface);
	result = g_strdup(buf->str);
	string_free(buf);
	interface_free(iface);
    
    return result;
}

static gboolean split_query(const gchar *query, gchar **include, gchar **exclude)
{
    gchar		*foo;
    gchar		**token;
    gint		i;
    gint		j;

    token = g_strsplit(query, " ", 0);
    
    for (i = 0; token[i]; i++) {
		g_strstrip(token[i]);

		foo = g_strndup(token[i], 2);
	
		if (token[i][0] == '-') {
			if (*exclude)
				*exclude = strconcat(*exclude, " ", token[i] + 1, NULL);
			else
				*exclude = g_strdup(token[i] + 1);
		} else {
			j = (strcmp(foo, "\\-")) ? 0 : 2;
	    
			if (*include)
				*include = strconcat(*include, " ", token[i] + j, NULL);
			else
				*include = g_strdup(token[i]);
		}
    }
    
    g_strfreev(token);

    return FALSE;
}

gboolean start_search(gpointer entry, SearchMode mode)
{
	gchar		*query = NULL;
	gchar		*include = NULL;
	gchar		*exclude = NULL;
	gchar		*msg = NULL;
	const gchar	*tmp = NULL;
    
    gtk_tree_store_clear(stores[STORE_SEARCH]);
    
	tmp = gtk_entry_get_text(GTK_ENTRY(entry));

    if (!strlen(tmp))
		return TRUE;

	query = utf8_to_locale(tmp);

	if (!query) {
#ifdef GIFTOXIC_DEBUG
		g_critical("Couldn't convert buffer from GtkTextEntry!\n");
#endif
		return TRUE;
	}
    
    escape(&query);
    g_strstrip(query);
    
    split_query(query, &include, &exclude);
    g_free(query);
    
	stop_search(); 
    msg = build_final_search_string(include, exclude, mode);
    
	g_free(include);
    g_free(exclude);
    
    if (msg) {
		running_searches++;
		statusbar_set(STATUSBAR_MISC, _("Search: in progress..."));
		gift_send_str(msg);
		g_free(msg);
    }

    return FALSE;
}

gboolean start_source_search(const gchar *hash)
{
	gchar buf[8];
	
	snprintf(buf, sizeof(buf), "%i", event_id_get_new(EVENT_SRC_SEARCH));
	gift_send(2, "LOCATE", buf, "query", hash);
    
    return FALSE;
}

/* add a search result item to the TreeView
 * if a file with the same hash as the one in <search>, we add <search> as a child to the top level element with the same hash
 * if this hash isn't found in the tree, we simply append the item
 * returns TRUE if result item has been added or FALSE if it hasn't (dupe)
 *
 * FIXME: this function is horrible... it should really be rewritten.
 */
gboolean add_searchresult(SearchResult *item)
{
    GtkTreeModel    *tree_model;
    GtkTreeIter		iter;
    GtkTreeIter		iter_top;
    GtkTreeIter		iter_child;
    gchar		    *filename;
    gchar		    *hash;
    gchar		    *hash_in_tree;
    gchar		    *url;
    gchar		    *user;
	gchar			*user_unique;
    gchar		    *user_in_tree;
    gchar		    *mime;
    gchar		    *bitrate;
    gchar			*visible_size;
	gchar			*tmp;
    gboolean		added_element = FALSE;
    gboolean		tree_not_empty;
    gboolean		avail;
    gboolean		node_has_children;
    gboolean		dupe = FALSE;
    gulong		    filesize;
    gint		    sources;
    
    g_assert(item);

    added_element = FALSE;
    
    sources = 0;
    filesize = item->filesize;
    avail = item->availability;
	filename = locale_to_utf8(item->filename);
    url = locale_to_utf8(item->url);
    user_unique = locale_to_utf8(item->user);
	
	tmp = get_user_from_ident(item->user);
	user = locale_to_utf8(tmp);
	g_free(tmp);
	
    hash = g_strdup(item->hash);
	if (!hash) return FALSE;
	
    mime = g_strdup(item->mime);
    bitrate = g_strdup(item->bitrate);
    visible_size = g_strdup("0 KB");

    tree_model = GTK_TREE_MODEL(stores[STORE_SEARCH]);
    tree_not_empty = gtk_tree_model_get_iter_first(tree_model, &iter);

    iter_top = iter;
    
    /* now iter, iter_bak, iter_top are pointing it at the first top level node */
    while (tree_not_empty && !added_element) {
		iter = iter_top;
        gtk_tree_model_get(tree_model, &iter, SUSER_UNIQUE, &user_in_tree, SHASH, &hash_in_tree, -1);

		if (hash_in_tree) {
			if (!strcasecmp(hash, hash_in_tree)) {
				
				/* hashes match, so maybe we should add our new search result here */
				if (!strcmp_utf8(user_unique, user_in_tree)) {
					/* the hash on top level is the same, but the user is too. => don't add */
					added_element = TRUE;
					dupe = TRUE;
					
				} else {
					
					/* check all child nodes, whether the user matches there. if not, add the new stuff */
					node_has_children = gtk_tree_model_iter_children(tree_model, &iter_child, &iter);
		
					while (node_has_children) {
						
						g_free(user_in_tree);
						gtk_tree_model_get(tree_model, &iter_child, SUSER, &user_in_tree, -1);
				
						if (!strcmp_utf8(user, user_in_tree)) {
							added_element = TRUE;
							dupe = TRUE;
							break;
						}
				
						node_has_children = gtk_tree_model_iter_next(tree_model, &iter_child);
					}
		
					if (!added_element){
				
						g_free(visible_size);
						visible_size = size_human(filesize);
					
						gtk_tree_store_append(stores[STORE_SEARCH], &iter, &iter_top); /* iter = child; iter_top = parent */
						gtk_tree_store_set(stores[STORE_SEARCH], &iter, SROOT, FALSE, SNAME, filename, SSIZE, filesize, SSIZEHUMAN, visible_size, SUSER, user, SAVAIL, avail, SHASH, hash, SURL, url, SMIME, mime, SBITRATE, bitrate, -1);
			
						/* update the number of sources in the parent item */
						gtk_tree_model_get(tree_model, &iter_top, SSOURCES, &sources, -1);
						sources++;
						gtk_tree_store_set(stores[STORE_SEARCH], &iter_top, SSOURCES, sources, -1);
						added_element = TRUE;
						dupe = FALSE;
					}
				}
			}
		} 
		g_free(user_in_tree);
		g_free(hash_in_tree);
		tree_not_empty = gtk_tree_model_iter_next(tree_model, &iter_top);
    }
    
    if (!added_element) {
		g_free(visible_size);
		visible_size = size_human(filesize);

        /* tree is empty, so we append our new stuff at the end */
        gtk_tree_store_append(stores[STORE_SEARCH], &iter, NULL);
        gtk_tree_store_set(stores[STORE_SEARCH], &iter, SROOT, TRUE, SSOURCES, 1, SNAME, filename, SSIZE, filesize, SSIZEHUMAN, visible_size, SUSER, user, SAVAIL, avail, SUSER_UNIQUE, user_unique, SHASH, hash, SURL, url, SMIME, mime, SBITRATE, bitrate, -1);
		dupe = FALSE;
    }

    g_free(filename);
    g_free(hash);
    g_free(user);
	g_free(user_unique);
    g_free(url);
    g_free(mime);
    g_free(bitrate);
	g_free(visible_size);

    return dupe;
}

void browse_users_files(GtkMenuItem *menu_item, Moo *data)
{
    gtk_tree_path_free(data->path);
	g_free(data);
}

/* returns the number of entries in the search treestore (multiple sources are not counted) */
gulong get_search_results_num()
{
	GtkTreeModel    *tree_model;
	GtkTreeIter     iter;
	gulong          entries = 0;
	gboolean        entries_left;
	
	tree_model = GTK_TREE_MODEL(stores[STORE_SEARCH]);
	entries_left = gtk_tree_model_get_iter_first(tree_model, &iter);

	while (entries_left) {
		entries++;
		entries_left = gtk_tree_model_iter_next(tree_model, &iter);
	}

	return entries;
}

