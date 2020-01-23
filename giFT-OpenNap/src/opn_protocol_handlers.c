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
#include <libgift/proto/share.h>
#include "opn_download.h"
#include "opn_search.h"

OPN_HANDLER(login_error)
{
	assert(udata);

#ifdef OPENNAP_DEBUG
	printf("login err: %s\n", data);
#endif

	opn_session_free((OpnSession *) udata);
}

OPN_HANDLER(login_ack)
{
	OpnSession *session = (OpnSession *) udata;

	assert(session);
	
	session->state = OPN_SESSION_STATE_CONNECTED;
	session->node->state = OPN_NODE_STATE_ONLINE;
}

OPN_HANDLER(error)
{
#ifdef OPENNAP_DEBUG
	printf("general err: %s\n", data);
#endif
}

OPN_HANDLER(stats)
{
	OpnSession *session = (OpnSession *) udata;

	assert(session);
	memset(&session->stats, 0, sizeof(OpnStats));

	sscanf(data, "%lu %lu %lf",
	       (unsigned long *) &session->stats.users,
	       (unsigned long *) &session->stats.files,
	       &session->stats.size);
}

OPN_HANDLER(ping)
{
	OpnSession *session = (OpnSession *) udata;
	OpnPacket *packet;

	assert(session);

	if (!(packet = opn_packet_new(OPN_CMD_PONG, data, strlen(data))))
		return;

	opn_packet_send(packet, session->con);
	opn_packet_free(packet);
}

static char *get_filename(char *path)
{
	char *ptr;

	assert(path);

	if ((ptr = strrchr(path, '/')) && !string_isempty(++ptr))
		return ptr;
	else
		return NULL;
}

static char *get_directory(char *path)
{
	char buf[PATH_MAX + 1];

	assert(path);

	snprintf(buf, get_filename(path) - path, "%s", path);

	return strdup(buf);
}

/* temporary, until giFT's function is fixed */
char *my_file_unix_path (char *host_path)
{
	char *unix_path;
	char *ptr;

	if (!(unix_path = STRDUP(host_path)))
		return NULL;

	if (host_path[1] == ':') {
		/* C:\dir\file -> /C\dir\file */
		unix_path[0] = '/';
		unix_path[1] = host_path[0];
	}

	for (ptr = unix_path; *ptr; ptr++) {
		if (*ptr == '\\')
			*ptr = '/';
	}

	return unix_path;
}

OPN_HANDLER(search_result)
{
	OpnSession *session = (OpnSession *) udata;
	OpnUrl url;
	OpnSearch *search;
	Share share;
	char md5[33], user[64], bitrate[8], frequency[8], length[16];
	char tmp[PATH_MAX + 1];
	char *file, *path, *root;
	uint32_t ip, filesize;

	if (!data)
		return;

	assert(session);

	md5[0] = user[0] = bitrate[0] = frequency[0] = length[0] = 0;
	tmp[0] = 0;

	sscanf(data, "\"%[^\"]\" %32s %u %s %s %s %63s %u %*[^\n]", tmp, md5,
			&filesize, bitrate, frequency, length, user, &ip);

	if (!user)
		return;

	/* FIXME */
	path = my_file_unix_path(tmp);
	file = get_filename(path);
	root = get_directory(path);

	/* now find the search this searchresult might belong to
	 * .oO(stupid napster)
	 */
	if (!(search = opn_search_find(path)))
		return;
	
	assert(search->event);

	share_init(&share, file);
	share_set_root(&share, root, strlen(root));
	share.size = filesize;
	share_set_meta(&share, "Bitrate", bitrate);
	share_set_meta(&share, "Frequency", frequency);
	share_set_meta(&share, "Length", length);

	opn_url_set_file_data(&url, file, filesize);
	opn_url_set_client_data(&url, user, ip, 0);
	opn_url_set_server_data(&url, session->node->ip,
			session->node->port);

	opn_proto->search_result(opn_proto, search->event,
			user, NULL, opn_url_serialize(&url),
			1, &share);

	share_finish(&share);
}

OPN_HANDLER(search_finished)
{
	OpnSearch *search;

	if (!OPENNAP->searches)
		return;

	if (list_length(OPENNAP->searches) == 1) {
		search = (OpnSearch *) OPENNAP->searches->data;

		opn_search_unref(search);
	} else {
		/* FIXME */
	}
}

OPN_HANDLER(download_ack)
{
	OpnDownload *download;
	OpnUrl url;
	char user[64], file[PATH_MAX + 1];
	in_addr_t ip;
	in_port_t port;

	sscanf(data, "%63s %u %hu \"%[^\n]\"",
	       user, &ip, &port, file);

	opn_url_set_file_data(&url, file, 0);
	opn_url_set_client_data(&url, user, ip, port);

	/* if port is 0 => user is firewalled
	 * currently not supported
	 * FIXME implement me!
	 */
	if (!(download = opn_download_find(&url)) || !port)
		return;

	opn_download_start(download);
}

