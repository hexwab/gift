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

gint strcmp_utf8(gchar *, gchar *);
gint strcasecmp_utf8(gchar *, gchar *);

void free_searchresult(gpointer, gpointer);
void free_transfer(Transfer *);
void free_chunk(Chunk *);

void escape(gchar **);
void unescape(gchar **);

gchar *my_strstr(gchar *, const gchar *);
gint my_strtoul(gchar *);
gchar *get_user_from_ident(gchar *);
gchar *get_filename_from_path(gchar *);
gchar *get_dir_from_path(gchar *);
gint hex_to_dec(gchar *);
void url_decode(gchar **);
gchar *strconcat(gchar *, ...);
gchar *number_human(gulong size);
gchar *size_human(gulong size);
gchar *time_human(gulong);
void show_dialog(const gchar *, GtkMessageType);
void parse_parameters(gint, gchar **, Options **);

Options *read_settings();
void apply_settings(Options *);
void save_settings(Options *);

gchar *locale_to_utf8(const gchar *);
gchar *utf8_to_locale(const gchar *);

gboolean is_local_host(const gchar *host);

