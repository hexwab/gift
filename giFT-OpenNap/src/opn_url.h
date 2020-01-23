/* giFT OpenNap
 * Copyright (C) 2003 Tilman Sauerbeck <tilman@code-monkey.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __OPN_URL_H
#define __OPN_URL_H

typedef struct {
	struct {
		in_addr_t ip;
		in_port_t port;
	} client;

	struct {
		in_addr_t ip;
		in_port_t port;
	} server;

	char user[64];
	char file[PATH_MAX + 1];
	uint32_t size;

	char serialized[PATH_MAX + 256];
} OpnUrl;

OpnUrl *opn_url_new();

void opn_url_set_file_data(OpnUrl *url, char *file, uint32_t size);
void opn_url_set_client_data(OpnUrl *url, char *user, in_addr_t ip,
                             in_port_t port);

void opn_url_set_server_data(OpnUrl *url, in_addr_t ip, in_port_t port);

OpnUrl *opn_url_unserialize(char *data);
char *opn_url_serialize(OpnUrl *url);
void opn_url_free(OpnUrl *url);

#endif

