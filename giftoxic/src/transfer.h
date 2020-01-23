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

gboolean reset_speed_cols(gpointer);
gboolean chunk_add(Chunk *);
gboolean transfer_add(Transfer *);
gboolean transfer_get_sources_all(gpointer);
gboolean transfer_add_sources(GSList *);
void transfer_remove(TreeStore, gint, ...);
TransferStatus get_transfer_status(EventID *);
gboolean set_transfer_status(EventID *, TransferStatus);
gboolean get_transfer_num(gint *, gint *);
gboolean set_transfer_id(EventID *, gint);

/* menu callbacks */
void transfer_remove_with_path(GtkMenuItem *, Moo *);
void transfer_remove_all(GtkMenuItem *, Moo *);
void transfer_pause(GtkMenuItem *, Moo *);
void transfer_resume(GtkMenuItem *, Moo *);
void transfer_cancel(GtkMenuItem *, Moo *);
void transfer_get_sources(GtkMenuItem *, Moo *);
void file_download(GtkMenuItem *, GtkTreePath *);
