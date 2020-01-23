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

typedef enum
{
	/* network */
	STAT_NW_USERS,
	STAT_NW_FILES,
	STAT_NW_VOL,
	STAT_NW_NETWORKS,
	/* local */
	STAT_LOCAL_FILES,
	STAT_LOCAL_VOL,
	STAT_NUM
} StatInfo;

gboolean stats_get(gpointer);
void stats_update(Stats *);
void statusbar_set(Statusbar, gchar *fmt, ...);
GtkWidget *create_statusbar();
GtkWidget *create_home_tab();

