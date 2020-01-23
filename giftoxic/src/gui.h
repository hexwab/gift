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

#define COL_NAME_WIDTH 300

typedef enum
{
	STORE_DOWNLOAD,
	STORE_UPLOAD,
	STORE_SEARCH,
	STORE_NUM
} TreeStore;

extern GtkTreeStore	*stores[STORE_NUM]; /* download, upload and search treestore */
gboolean show_context_menu(GtkTreeView *treeview, GdkEventButton *event, gint store);
