/*
 * Copyright (C) 2003 Markus Kern (mkern@users.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "fst_fasttrack.h"
#include "fst_download.h"

/*****************************************************************************/

#define DOWNLOAD_BUF_SIZE 4069

/*****************************************************************************/

// called by gift to start downloading of a chunk
int gift_cb_download_start (Protocol *p, Transfer *transfer, Chunk *chunk, Source *source)
{
	FSTDownload *download = fst_download_create (chunk);
	fst_download_start (download);

	return TRUE;
}

// called by gift to stop download
void gift_cb_download_stop (Protocol *p, Transfer *transfer, Chunk *chunk, Source *source, int complete)
{
	FSTDownload *download;

	if(!chunk || !chunk->data)
	{
		FST_HEAVY_DBG ("gift_cb_download_stop: chunk->data == NULL, no action taken");
		return;
	}

	download = (FSTDownload*)chunk->data;

	if(complete)
	{
//		FST_DBG_2 ("removing completed download from %s:%d", net_ip_str(download->ip), download->port);
		FST_PROTO->source_status (FST_PROTO, chunk->source, SOURCE_COMPLETE, "complete");
		fst_download_stop (download);
	}
	else
	{
//		FST_DBG_2 ("removing cancelled download from %s:%d", net_ip_str(download->ip), download->port);
		FST_PROTO->source_status (FST_PROTO, chunk->source, SOURCE_CANCELLED, "cancelled");
		fst_download_stop (download);
	}
}

// called by gift to remove source
int gift_cb_source_remove (Protocol *p, Transfer *transfer, Source *source)
{
	FSTDownload *download;

	if(!source || !source->chunk || !source->chunk->data)
	{
		FST_DBG ("gift_cb_source_remove: invalid source, no action taken");
		return FALSE;
	}

	download = (FSTDownload*)source->chunk->data;

	FST_DBG_2 ("removing source %s:%d", net_ip_str(download->ip), download->port);
	FST_PROTO->source_status (FST_PROTO, source->chunk->source, SOURCE_CANCELLED, "cancelled");
	fst_download_stop (download);

	return TRUE;
}

/*****************************************************************************/

static void download_connected(int fd, input_id input, FSTDownload *download);
static void download_read_header(int fd, input_id input, FSTDownload *download);
static void download_read_body(int fd, input_id input, FSTDownload *download);
static void download_write_gift (FSTDownload *download, unsigned char *data, unsigned int len);
static void download_error_gift (FSTDownload *download, int remove_source, unsigned short klass, char *error);
static char *download_parse_url (char *url, unsigned int *ip, unsigned short *port);

/*****************************************************************************/

// alloc and init download
FSTDownload *fst_download_create (Chunk *chunk)
{
	FSTDownload *dl = malloc (sizeof (FSTDownload));

	dl->state = DownloadNew;
	dl->tcpcon = NULL;
	dl->in_packet = fst_packet_create ();

	dl->chunk = chunk;
	dl->uri = download_parse_url (chunk->source->url, &dl->ip, &dl->port);

	// make chunk refer back to us
	chunk->data = (void*)dl;
	
	return dl;
}

// free download, stop it if necessary
void fst_download_free (FSTDownload *download)
{
	if(!download)
		return;

	tcp_close(download->tcpcon);
	fst_packet_free (download->in_packet);
	free (download->uri);

	// unref chunk
	if(download->chunk)
		download->chunk->data = NULL;

	free (download);
}

// start download
int fst_download_start (FSTDownload *download)
{
	if(!download || download->state != DownloadNew || !download->chunk)
		return FALSE;

	FST_HEAVY_DBG_2 ("connecting to %s:%d", net_ip_str(download->ip), download->port);

	download->state = DownloadConnecting;
	FST_PROTO->source_status (FST_PROTO, download->chunk->source, SOURCE_WAITING, "connecting");

	download->tcpcon = tcp_open (download->ip, download->port, FALSE);
	if(download->tcpcon == NULL)
	{
		download_error_gift (download, TRUE, SOURCE_TIMEOUT, "connection failed");
		return FALSE;
	}

	download->tcpcon->udata = (void*)download;

	// wait for connected
	input_add (download->tcpcon->fd, (void*)download, INPUT_WRITE, (InputCallback) download_connected, FST_DOWNLOAD_CONNECT_TIMEOUT);

	return TRUE;
}

// stop download
int fst_download_stop (FSTDownload *download)
{
	fst_download_free (download);

	return TRUE;
}

/*****************************************************************************/

static void download_connected(int fd, input_id input, FSTDownload *download)
{
	FSTHttpRequest *request;
	FSTPacket *packet;
	char buf[64];

	input_remove (input);

	if (net_sock_error (download->tcpcon->fd))
	{
		FST_DBG_2 ("connection to %s:%d failed -> removing source", net_ip_str(download->ip), download->port);
		download_error_gift (download, TRUE, SOURCE_TIMEOUT, "connection failed");
		return;
	}

	download->state = DownloadRequesting;
	FST_PROTO->source_status (FST_PROTO, download->chunk->source, SOURCE_WAITING, "requesting");

	// create http request
	request = fst_http_request_create ("GET", download->uri);
	// add http headers
	fst_http_request_set_header (request, "UserAgent", "giFT-FastTrack");
	fst_http_request_set_header (request, "Connection", "close");
	fst_http_request_set_header (request, "X-Kazaa-Network", FST_NETWORK_NAME);
	fst_http_request_set_header (request, "X-Kazaa-Username", FST_USER_NAME);
	// host
	sprintf (buf, "%s:%d", net_ip_str (download->ip), download->port);
	fst_http_request_set_header (request, "Host", buf);
	// range, http range is inclusive!
	// IMPORTANT: use chunk->start + chunk->transmit for startint point, rather non-intuitive
	sprintf (buf, "bytes=%d-%d", (int)(download->chunk->start + download->chunk->transmit), (int)download->chunk->stop - 1);

	fst_http_request_set_header (request, "Range", buf);

	// compile and send request
	packet = fst_packet_create ();
	fst_http_request_compile (request, packet);
	fst_http_request_free (request);

	if(fst_packet_send (packet, download->tcpcon) == FALSE)
	{
		download_error_gift (download, FALSE, SOURCE_TIMEOUT, "request failed");
		fst_packet_free (packet);
		return;
	}

	fst_packet_free (packet);

	// wait for header
	input_add (download->tcpcon->fd, (void*)download, INPUT_READ, (InputCallback) download_read_header, FST_DOWNLOAD_HANDSHAKE_TIMEOUT);
}

static void download_read_header(int fd, input_id input, FSTDownload *download)
{
	FSTHttpReply *reply;
	char *p;

	input_remove (input);

	if (net_sock_error (download->tcpcon->fd))
	{
		FST_DBG_2 ("read error while downloading from %s:%d -> removing source", net_ip_str(download->ip), download->port);
		download_error_gift (download, TRUE, SOURCE_TIMEOUT, "request failed");
		return;
	}

	// read data
	if(fst_packet_recv (download->in_packet, download->tcpcon) == FALSE)
	{
		FST_DBG_2 ("read error while getting header from %s:%d -> aborting", net_ip_str(download->ip), download->port);
		download_error_gift (download, TRUE, SOURCE_TIMEOUT, "request failed");
		return;
	}

	reply = fst_http_reply_create ();

	if(fst_http_reply_parse (reply, download->in_packet) == FALSE)
	{	
		fst_http_reply_free (reply);

		if(fst_packet_size (download->in_packet) > 4096)
		{
			FST_DBG ("ERROR: didn't get whole http header and received more than 4K, closing connection");
			download_error_gift (download, TRUE, SOURCE_TIMEOUT, "source sent crap");
			return;
		}

		FST_DBG_2 ("didn't get whole header from %s:%d -> waiting for more", net_ip_str(download->ip), download->port);
		// wait for rest of header
		input_add (download->tcpcon->fd, (void*)download, INPUT_READ, (InputCallback) download_read_header, FST_DOWNLOAD_HANDSHAKE_TIMEOUT);
		return;
	}

/*
	printf("\nhttp reply from %s: \n", net_ip_str(download->ip));
	print_bin_data(download->in_packet->data, fst_packet_size(download->in_packet) - fst_packet_remaining(download->in_packet));
*/

	if(reply->code != 200 && reply->code != 206) // 206 == partial content
	{
		FST_DBG_4 ("%s:%d replied with %d (\"%s\") -> aborting", net_ip_str(download->ip), download->port, reply->code, reply->code_str);

		if(reply->code == 503)
			download_error_gift (download, FALSE, SOURCE_QUEUED_REMOTE, "remotely queued");
		else if(reply->code == 404)
			download_error_gift (download, TRUE, SOURCE_CANCELLED, "file not found");
		else
			download_error_gift (download, TRUE, SOURCE_CANCELLED, "weird http code");

		fst_http_reply_free (reply);
		return;
	}

	FST_DBG_4 ("%s:%d replied with %d (\"%s\") -> downloading", net_ip_str(download->ip), download->port, reply->code, reply->code_str);

	// check that recevied ranges are correct
	if((p = fst_http_reply_get_header (reply, "content-range")))
	{
		int start, stop;
		sscanf (p, "bytes %d-%d", &start, &stop);

		if(start != download->chunk->start || stop != download->chunk->stop-1)
		{
			// longer ranges than requested should be ok since giFT handles this
			FST_DBG ("WARNING: requested range differs from receveived, file corruption may occur");
			FST_DBG_2 ("\trequested range: %d-%d", download->chunk->start, download->chunk->stop - 1);
			FST_DBG_2 ("\treceived range: %d-%d", start, stop);
			if((p = fst_http_reply_get_header (reply, "content-length")))
				FST_DBG_1 ("\tcontent-length: %s", p);

			FST_DBG ("WARNING: removing source due to range mismatch");
			fst_http_reply_free (reply);
			download_error_gift (download, TRUE, SOURCE_CANCELLED, "range mismatch");
			return;
		}
	} else {
		FST_DBG ("WARNING: server didn't sent content-range header");
		fst_http_reply_free (reply);
		download_error_gift (download, TRUE, SOURCE_CANCELLED, "missing content-range");
		return;
	}

	// update status
	download->state = DownloadRunning;
	FST_PROTO->source_status (FST_PROTO, download->chunk->source, SOURCE_ACTIVE, "active");
	// free reply
	fst_http_reply_free (reply);
	// remove header from packet
	fst_packet_truncate (download->in_packet);

	// wait for body
	input_add (download->tcpcon->fd, (void*)download, INPUT_READ, (InputCallback) download_read_body, FST_DOWNLOAD_DATA_TIMEOUT);

	// write rest of packet to file
	download_write_gift (download, download->in_packet->read_ptr, fst_packet_remaining (download->in_packet));
}

static void download_read_body(int fd, input_id input, FSTDownload *download)
{
	char *data;
	int len;

	if (net_sock_error (download->tcpcon->fd))
	{
		FST_DBG_2 ("read error while downloading from %s:%d -> aborting", net_ip_str(download->ip), download->port);
		// this makes giFT call gift_cb_download_stop(), which closes connection and frees download
		download_error_gift (download, FALSE, SOURCE_TIMEOUT, "download failed");
		return;
	}
	
	data = malloc (DOWNLOAD_BUF_SIZE);

	if((len = tcp_recv (download->tcpcon, data, DOWNLOAD_BUF_SIZE)) <= 0)
	{
		FST_DBG ("download_read_body: tcp_recv() <= 0");
		// this makes giFT call gift_cb_download_stop(), which closes connection and frees download
		download_error_gift (download, FALSE, SOURCE_CANCELLED, "download error");
		return;
	}
	
	// write data to file through giFT, this calls gift_cb_download_stop() if download is complete
	download_write_gift (download, data, len);

	free (data);
}

/*****************************************************************************/

static void download_write_gift (FSTDownload *download, unsigned char *data, unsigned int len)
{
	FST_PROTO->chunk_write (FST_PROTO, download->chunk->transfer, download->chunk, download->chunk->source, data, len);
}

static void download_error_gift (FSTDownload *download, int remove_source, unsigned short klass, char *error)
{
//	FST_DBG ("download_error_gift()");
	if(remove_source)
	{
		// hack to remove source from download
		FST_PROTO->source_status (FST_PROTO, download->chunk->source, klass, error);
//		download_remove_source (download->chunk->transfer, download->chunk->source->url);
//		download->chunk->source = NULL;
		// this makes giFT call gift_cb_download_stop()
		download_write_gift (download, NULL, 0);
	}
	else
	{

		FST_PROTO->source_status (FST_PROTO, download->chunk->source, klass, error);
		// this makes giFT call gift_cb_download_stop()
		download_write_gift (download, NULL, 0);
	}
}


// parses url, returns uri which caller frees or NULL on failure
static char *download_parse_url (char *url, unsigned int *ip, unsigned short *port)
{
	char *tmp, *uri, *ip_str, *port_str;

	if (!url)
		return NULL;

	tmp = uri = strdup (url);

	string_sep (&uri, "://");       /* get rid of this useless crap */

	/* divide the string into two sides */
	if ((port_str = string_sep (&uri, "/")))
	{
		/* pull off the left-hand operands */
		ip_str     = string_sep (&port_str, ":");

		if (ip_str && port_str)
		{
			if (ip)
				*ip = net_ip (ip_str);
			if (port)
				*port = ATOI (port_str);
		
			uri--; *uri = '/';
			uri = strdup (uri);
			free (tmp);
			return uri;
		}
	}

	free (tmp);
	return NULL;
}

/*****************************************************************************/
