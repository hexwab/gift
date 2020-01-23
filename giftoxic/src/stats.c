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
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "config.h"
#include "common.h"
#include "stats.h"
#include "io.h"
#include "utils.h"

GtkWidget	*stats_label[STAT_NUM];
GtkWidget	*statusbar[STATUSBAR_NUM];
guint		statusbar_context[STATUSBAR_NUM];

gboolean stats_get(gpointer data)
{
	gift_send(1, "STATS", NULL);

	return TRUE;
}

void stats_update(Stats *s)
{
	gchar   *label;
	gchar   *number;

	label = number_human(s->users);
	gtk_label_set_text(GTK_LABEL(stats_label[STAT_NW_USERS]), label); /* users online */
	g_free(label);
	
	label = number_human(s->files_local);
	gtk_label_set_text(GTK_LABEL(stats_label[STAT_LOCAL_FILES]), label); /* number of files shared by the user */
	g_free(label);

	if (s->size_local >= 1){
		number = number_human(s->size_local);
		label = g_strdup_printf(_("%s GB"), number);
	} else {
		number = number_human(s->size_local*1000);
		label = g_strdup_printf(_("%s MB"), number);
	}

	gtk_label_set_text(GTK_LABEL(stats_label[STAT_LOCAL_VOL]), label); /* size shared by the user */
	g_free(number);
	g_free(label);

	label = number_human(s->files_remote);
	gtk_label_set_text(GTK_LABEL(stats_label[STAT_NW_FILES]), label); /* number of files shared remotely */
	g_free(label);
	
	if (s->size_remote >= 1){
		number = number_human(s->size_remote);
		label = g_strdup_printf(_("%s GB"), number);
	} else {
		number = number_human(s->size_remote*1000);
		label = g_strdup_printf(_("%s MB"), number);
	}
	
	gtk_label_set_text(GTK_LABEL(stats_label[STAT_NW_VOL]), label); /* size shared remotely */
	g_free(number);
	g_free(label);

	label = g_strdup(s->connected_networks);
	gtk_label_set_text(GTK_LABEL(stats_label[STAT_NW_NETWORKS]), label);
	g_free(label);
}

void statusbar_set(Statusbar num, gchar *fmt, ...)
{
	va_list list;
	gchar   buf[1024];
	
	va_start(list, fmt);
	vsnprintf(buf, sizeof(buf), fmt, list);
	va_end(list);

	gtk_statusbar_pop(GTK_STATUSBAR(statusbar[num]), statusbar_context[num]);
	gtk_statusbar_push(GTK_STATUSBAR(statusbar[num]), statusbar_context[num], buf);
}

GtkWidget *create_statusbar()
{
	GtkWidget	*hbox = gtk_hbox_new(TRUE, 2);
	GtkWidget	*hbox2 = gtk_hbox_new(TRUE, 2);
	gchar		*ctx[STATUSBAR_NUM] = {"StatsTransfer", "StatsMisc"};
	gint		i;

	for (i = 0; i < STATUSBAR_NUM; i++) {
		statusbar[i] = gtk_statusbar_new();
		
		gtk_statusbar_set_has_resize_grip(GTK_STATUSBAR(statusbar[i]), FALSE);
		statusbar_context[i] = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar[i]), ctx[i]);
		
		if (!i)
			gtk_box_pack_start(GTK_BOX(hbox), statusbar[i], TRUE, TRUE, 0);
		else
			gtk_box_pack_start(GTK_BOX(hbox2), statusbar[i], TRUE, TRUE, 0);
	}
	
	gtk_box_pack_start(GTK_BOX(hbox), hbox2, TRUE, TRUE, 0);
	
	return hbox;
}

GtkWidget *create_home_tab()
{
	GtkWidget	*table;
	GtkWidget	*label;
	GtkWidget	*home_tab;
	GtkWidget	*img_logo;
	GtkWidget	*frame;
	GtkWidget	*vbox;
	GtkWidget	*vbox2;
	GtkWidget	*hbox2;
	GdkPixbuf	*pixbuf;
	gchar		buf[128];
	gchar		*label_captions[STAT_NUM] = {N_("Users online:"), N_("Files available:"), N_("Volume available:"), N_("Connected to:"),
                                      N_("Local files shared:"), N_("Local volume shared:")};
	gchar		*frame_captions[2] = {N_("Network"), N_("Local")};
	gint		i, j, k, x[2] = {STAT_NW_USERS, STAT_LOCAL_FILES}, y[2] = {STAT_NW_NETWORKS, STAT_LOCAL_VOL};
   
	home_tab = gtk_vbox_new(FALSE, 2);
	gtk_container_set_border_width(GTK_CONTAINER(home_tab), 10);

	/* about frame */
	frame = gtk_frame_new(NULL);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 0);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_box_pack_start(GTK_BOX(home_tab), frame, TRUE, TRUE, 50);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	
	if ((pixbuf = gdk_pixbuf_new_from_file(DATADIR "/logo.png", NULL)))
		img_logo = gtk_image_new_from_pixbuf(pixbuf);
	else
		img_logo = gtk_image_new_from_stock(GTK_STOCK_MISSING_IMAGE, GTK_ICON_SIZE_DIALOG);

	gtk_box_pack_start(GTK_BOX(vbox), img_logo, TRUE, TRUE, 5);
	
	/* show version information */
	snprintf(buf, sizeof(buf), _("Version %s\n\n(C) 2002, 2003 giFToxic team\n"), VERSION);
	label = gtk_label_new(buf);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);

	/* stats vbox */
	vbox2 = gtk_vbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(home_tab), vbox2, FALSE, FALSE, 5);
	
	hbox2 = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox2, TRUE, TRUE, 5);
	
	for (i = 0; i < 2; i++) {
		frame = gtk_frame_new(NULL);
		gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
		gtk_frame_set_shadow_type(GTK_FRAME (frame), GTK_SHADOW_NONE);
		gtk_box_pack_start(GTK_BOX(hbox2), frame, TRUE, TRUE, 5);
		
		snprintf(buf, sizeof(buf), "<span weight=\"bold\">%s</span>", _(frame_captions[i]));
		label = gtk_label_new(buf);
		gtk_frame_set_label_widget(GTK_FRAME(frame), label);
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	
		table = gtk_table_new(4, 2, TRUE);
		gtk_container_set_border_width(GTK_CONTAINER(table), 5);
		gtk_container_add(GTK_CONTAINER(frame), table);

		for (j = x[i], k = 0; j <= y[i]; j++, k++) {
			label = gtk_label_new(_(label_captions[j]));
			stats_label[j] = gtk_label_new(_("Getting stats..."));
			gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, k, k + 1);
			gtk_table_attach_defaults(GTK_TABLE(table), stats_label[j], 1, 2, k, k + 1);
		}
	}
	
	return home_tab;
}

