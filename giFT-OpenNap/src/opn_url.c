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

#include "opn_opennap.h"

OpnUrl *opn_url_new()
{
	OpnUrl *url;

	if (!(url = malloc(sizeof(OpnUrl))))
		return NULL;

	memset(url, 0, sizeof(OpnUrl));

	return url;
}

void opn_url_set_client_data(OpnUrl *url, char *user, in_addr_t ip,
                             in_port_t port)
{
	assert(url);

	url->client.ip = ip;
	url->client.port = port;
	snprintf(url->user, sizeof(url->user), "%s", user);
}

void opn_url_set_server_data(OpnUrl *url, in_addr_t ip, in_port_t port)
{
	assert(url);

	url->server.ip = ip;
	url->server.port = port;
}

void opn_url_set_file_data(OpnUrl *url, char *file, uint32_t size)
{
	assert(url);

	url->size = size;
	snprintf(url->file, sizeof(url->file), "%s", file);
}

OpnUrl *opn_url_unserialize(char *data)
{
	OpnUrl *url;

	assert(data);

	if (!(url = malloc(sizeof(OpnUrl))))
		return NULL;

	memset(url, 0, sizeof(OpnUrl));

	sscanf(data, "%u:%hu\n%u:%hu\n%s\n%s\n%u", &url->server.ip,
	       &url->server.port, &url->client.ip, &url->client.port,
	       url->user, url->file, &url->size);

	return url;
}

char *opn_url_serialize(OpnUrl *url)
{
	assert(url);

	snprintf(url->serialized, sizeof(url->serialized),
	         "%u:%hu\n%u:%hu\n%s\n%s\n%u",
	         url->server.ip, url->server.port, url->client.ip,
	         url->client.port, url->user, url->file, url->size);

	return url->serialized;
}

void opn_url_free(OpnUrl *url)
{
	free(url);
}

