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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include "common.h"
#include "parse.h"
#include "utils.h"

GIOChannel *iochan = NULL;
gint		sock = -1;

void gift_send_str(gchar *input)
{
	gchar	*msg;
	
	if (!iochan)
		return;

#ifdef GIFTOXIC_DEBUG
	printf("out: %s", input);
#endif

	msg = locale_to_utf8(input);

	if (msg) {
		g_io_channel_write_chars(iochan, msg, strlen(msg), NULL, NULL);
		g_io_channel_flush(iochan, NULL);
		
		g_free(msg);
	}

	return;
}

void gift_send(gint count, ...)
{
	Interface	*iface;
	String		*buf;
	va_list		list;
	gchar		*key;
	gchar		*value;
	gint		i;

	if (!iochan)
		return;

	va_start(list, count);

	key = va_arg(list, gchar *);
	value = va_arg(list, gchar *);
	
	iface = interface_new(key, value);

	for (i = 1; i < count; i++) {
		key = va_arg(list, gchar *);
		value = va_arg(list, gchar *);
	
		interface_put(iface, key, value);
	}
	
	va_end(list);

	buf = interface_serialize(iface);
	interface_free(iface);
	
	gift_send_str(buf->str);
	string_free(buf);

	return;
}

gboolean daemon_input_read(GIOChannel *unused, GIOCondition condition, gpointer data)
{
	gchar		*str = NULL;
	gchar		*locale;
	
	switch (g_io_channel_read_line(iochan, &str, NULL, NULL, NULL)) {
		case G_IO_STATUS_NORMAL:
			locale = utf8_to_locale(str);

			if (locale) {
#ifdef GIFTOXIC_DEBUG
				printf("in: %s\n", locale);
#endif
				parse_daemon_msg(locale);
				g_free(locale);
			}
			
			g_free(str);
			break;
		case G_IO_STATUS_EOF:
			g_io_channel_unref(iochan);
			iochan = NULL;
			
			return FALSE;
			break;
		default:
			break;
	}

	return TRUE;
}

void io_watch_create()
{
	GSource		*watch;
	gchar		line_term[] = {59, 10};	/* ';\n' */

	iochan = g_io_channel_unix_new(sock);
	g_io_channel_set_buffer_size(iochan, 0);
	g_io_channel_set_line_term(iochan, line_term, 2);
	g_io_channel_set_encoding(iochan, "ISO-8859-1", NULL);
	g_io_channel_set_close_on_unref(iochan, TRUE);
	
	watch = g_io_create_watch(iochan, G_IO_IN | G_IO_PRI);
	g_source_set_priority(watch, G_PRIORITY_LOW);
	g_source_set_callback(watch, (GSourceFunc) daemon_input_read, NULL, NULL);
	g_source_attach(watch, g_main_context_default());
	
	return;
}

gboolean daemon_connection_init(gchar *server, gint port)
{
	struct sockaddr_in	addr;
	struct hostent		*host = NULL;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) != -1) {
		host = gethostbyname(server);

		if (!host)
			return FALSE;
		
		addr.sin_addr = *(struct in_addr *) host->h_addr;
		addr.sin_port = g_htons(port);
		addr.sin_family = AF_INET;

		if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) != -1)
			return TRUE;
	}

	return FALSE;
}
