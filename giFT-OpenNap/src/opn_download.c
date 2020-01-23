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

#include <ctype.h>
#include "opn_opennap.h"
#include "opn_download.h"

BOOL gift_cb_download_start(Protocol *p, Transfer *transfer, Chunk *chunk, Source *source)
{
	OpnDownload *download;
	OpnSession *session;
	OpnPacket *packet;
	char buf[PATH_MAX + 128];
	BOOL ret;

	if (!(download = malloc(sizeof(OpnDownload))))
		return FALSE;

	download->chunk = chunk;
	download->url = opn_url_unserialize(source->url);

	if (!(session = opn_session_find(download->url))) {
		opn_download_free(download);
		return FALSE;
	}

	OPENNAP->downloads = list_prepend(OPENNAP->downloads, download);
	
	snprintf(buf, sizeof(buf), "%s \"%s\"", download->url->user,
	         download->url->file);

	if (!(packet = opn_packet_new(OPN_CMD_DOWNLOAD_REQUEST,
	                              buf, strlen(buf))))
		return FALSE;

	ret = opn_packet_send(packet, session->con);
	opn_packet_free(packet);
	
	return ret;
}

int gift_cb_source_remove(Protocol *p, Transfer *transfer, Source *source)
{
	return 0;
}

void gift_cb_download_stop(Protocol *p, Transfer *transfer, Chunk *chunk, Source *source, int complete)
{
	assert(OPENNAP->downloads);
}

OpnDownload *opn_download_new()
{
	OpnDownload *dl;

	if (!(dl = malloc(sizeof(OpnDownload))))
		return NULL;

	memset(dl, 0, sizeof(OpnDownload));

	return dl;
}

void opn_download_free(OpnDownload *dl)
{
	if (!dl)
		return;

	if (dl->con)
		tcp_close(dl->con);

	free(dl);
}

static void on_download_read_data(int fd, input_id input, OpnDownload *download)
{
	unsigned char buf[2048];
	int bytes;
	
	if (net_sock_error(fd)) {
		opn_download_free(download);
		return;
	}

	if ((bytes = tcp_recv(download->con, buf, sizeof(buf))) <= 0)
		opn_download_free(download);

	opn_proto->chunk_write(opn_proto, download->chunk->transfer,
	                       download->chunk, download->chunk->source, buf, bytes);
}

static void on_download_read_filesize(int fd, input_id input, OpnDownload *download)
{
	unsigned char buf[128];
	int bytes, i;
	uint32_t size = 0;
	
	if (net_sock_error(fd)) {
		opn_download_free(download);
		return;
	}

	input_remove(input);

	/* get the filesize */
	if ((bytes = tcp_peek(download->con, buf, sizeof(buf))) <= 0) {
		opn_download_free(download);
		return;
	}

	buf[bytes] = 0;

	for (i = 0; isdigit(buf[i]) && size < download->url->size; i++)
		size = (size * 10) + (buf[i] - '0');

	tcp_recv(download->con, buf, i);

	input_add(fd, download, INPUT_READ,
	          (InputCallback) on_download_read_data, TIMEOUT_DEF);
}

static void on_download_write(int fd, input_id input, OpnDownload *download)
{
	char buf[PATH_MAX + 256];
	
	if (net_sock_error(fd)) {
		opn_download_free(download);
		return;
	}

	input_remove(input);

	tcp_send(download->con, "GET", 3);

	snprintf(buf, sizeof(buf), "%s \"%s\" %lu",
	         OPENNAP_USERNAME, download->url->file,
	         download->chunk->start + download->chunk->transmit);

	tcp_send(download->con, buf, strlen(buf));

	input_add(fd, download, INPUT_READ,
	          (InputCallback) on_download_read_filesize, TIMEOUT_DEF);
}

static void on_download_connect(int fd, input_id input, OpnDownload *download)
{
	char c;
	
	if (net_sock_error(fd)) {
		opn_download_free(download);
		return;
	}

	input_remove(input);

	if (tcp_recv(download->con, &c, 1) <= 0 || c != '1')
		opn_download_free(download);

	input_add(fd, download, INPUT_WRITE,
	          (InputCallback) on_download_write, TIMEOUT_DEF);
}

void opn_download_start(OpnDownload *download)
{
	assert(download);

	if (!(download->con = tcp_open(download->url->client.ip,
	                               download->url->client.port, FALSE)))
		return;
	
	input_add(download->con->fd, download, INPUT_READ,
	          (InputCallback) on_download_connect, TIMEOUT_DEF);
}

OpnDownload *opn_download_find(OpnUrl *url)
{
	OpnDownload *download;
	List *l;

	assert(url);

	for (l = OPENNAP->downloads; l; l = l->next) {
		download = (OpnDownload *) l->data;

		if (!strcasecmp(url->file, download->url->file)
		    && !strcasecmp(url->user, download->url->user)
		    && url->client.ip == download->url->client.ip)
			return download;
	}

	return NULL;
}

