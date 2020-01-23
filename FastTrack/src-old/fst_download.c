/*
 * $Id: fst_download.c,v 1.13 2003/07/07 22:22:06 beren12 Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

/* called by gift to start downloading of a chunk */
int gift_cb_download_start (Protocol *p, Transfer *transfer, Chunk *chunk, Source *source)
{
	FSTDownload *download = fst_download_create (chunk);
	fst_download_start (download);

	return TRUE;
}

/* called by gift to stop download */
void gift_cb_download_stop (Protocol *p, Transfer *transfer, Chunk *chunk, Source *source, int complete)
{
	FSTDownload *download;

	if(!chunk || !chunk->udata)
	{
		FST_HEAVY_DBG ("gift_cb_download_stop: chunk->udata == NULL, no action taken");
		return;
	}

	download = (FSTDownload*)chunk->udata;

	if (complete)
	{
		FST_HEAVY_DBG_2 ("removing completed download from %s:%d", net_ip_str(download->ip), download->port);
		FST_PROTO->source_status (FST_PROTO, chunk->source, SOURCE_COMPLETE, "Complete");
		fst_download_stop (download);
	}
	else
	{
		FST_HEAVY_DBG_2 ("removing cancelled download from %s:%d", net_ip_str(download->ip), download->port);
		FST_PROTO->source_status (FST_PROTO, chunk->source, SOURCE_CANCELLED, "Cancelled");
		fst_download_stop (download);
	}
}

/* called by gift to remove source */
int gift_cb_source_remove (Protocol *p, Transfer *transfer, Source *source)
{
	FSTDownload *download;

	if (!source || !source->chunk || !source->chunk->udata)
	{
		FST_DBG ("gift_cb_source_remove: invalid source, no action taken");
		return FALSE;
	}

	download = (FSTDownload*)source->chunk->udata;

	FST_DBG_2 ("removing source %s:%d", net_ip_str(download->ip), download->port);
	FST_PROTO->source_status (FST_PROTO, source->chunk->source, SOURCE_CANCELLED, "Cancelled");
	fst_download_stop (download);

	return TRUE;
}

/*****************************************************************************/

static void download_connected (int fd, input_id input, FSTDownload *download);
static void download_read_header (int fd, input_id input, FSTDownload *download);
static void download_read_body (int fd, input_id input, FSTDownload *download);
static void download_write_gift (FSTDownload *download, unsigned char *data, unsigned int len);
static void download_error_gift (FSTDownload *download, int remove_source, unsigned short klass, char *error);
static char *download_parse_url (char *url, unsigned int *ip, unsigned short *port);
static char *download_calc_xferuid (char *uri);

/*****************************************************************************/

// alloc and init download
FSTDownload *fst_download_create (Chunk *chunk)
{
	FSTDownload *dl = malloc (sizeof (FSTDownload));

	dl->state = DownloadNew;
	dl->tcpcon = NULL;
	dl->in_packet = fst_packet_create();

	dl->chunk = chunk;
	dl->uri = download_parse_url (chunk->source->url, &dl->ip, &dl->port);

	/* make chunk refer back to us */
	chunk->udata = (void*)dl;
	
	return dl;
}

/* free download, stop it if necessary */
void fst_download_free (FSTDownload *download)
{
	if (!download)
		return;

	tcp_close (download->tcpcon);
	fst_packet_free (download->in_packet);
	free (download->uri);

	/* unref chunk */
	if (download->chunk)
		download->chunk->udata = NULL;

	free (download);
}

// start download
int fst_download_start (FSTDownload *download)
{
	if (!download || download->state != DownloadNew || !download->chunk)
		return FALSE;

	FST_HEAVY_DBG_2 ("connecting to %s:%d", net_ip_str(download->ip), download->port);

	download->state = DownloadConnecting;
	FST_PROTO->source_status (FST_PROTO, download->chunk->source, SOURCE_WAITING, "Connecting");

	download->tcpcon = tcp_open (download->ip, download->port, FALSE);
	if (download->tcpcon == NULL)
	{
		FST_DBG_2 ("ERROR: tcp_open() failed for %s:%d", net_ip_str(download->ip), download->port);
		download_error_gift (download, FALSE, SOURCE_TIMEOUT, "Connect failed");
		return FALSE;
	}

	download->tcpcon->udata = (void*)download;

	/* wait for connected */
	input_add (download->tcpcon->fd, (void*)download, INPUT_WRITE, (InputCallback)download_connected, FST_DOWNLOAD_CONNECT_TIMEOUT);

	return TRUE;
}

/* stop download */
int fst_download_stop (FSTDownload *download)
{
	fst_download_free (download);

	return TRUE;
}

/*****************************************************************************/

static void download_connected (int fd, input_id input, FSTDownload *download)
{
	FSTHttpRequest *request;
	FSTPacket *packet;
	char buf[64];

	input_remove (input);

	if (net_sock_error (download->tcpcon->fd))
	{
		FST_HEAVY_DBG_2 ("connection to %s:%d failed -> removing source", net_ip_str(download->ip), download->port);
		download_error_gift (download, TRUE, SOURCE_TIMEOUT, "Connect failed");
		return;
	}

	download->state = DownloadRequesting;
	FST_PROTO->source_status (FST_PROTO, download->chunk->source, SOURCE_WAITING, "Requesting");

	/* create http request */
	request = fst_http_request_create ("GET", download->uri);

	/* add http headers */
	fst_http_request_set_header (request, "UserAgent", "giFT-FastTrack");
	fst_http_request_set_header (request, "Connection", "close");
	fst_http_request_set_header (request, "X-Kazaa-Network", FST_NETWORK_NAME);
	fst_http_request_set_header (request, "X-Kazaa-Username", FST_USER_NAME);

#ifdef FST_DOWNLOAD_BOOST_PL
	fst_http_request_set_header (request, "X-Kazaa-XferUid", download_calc_xferuid(download->uri));
#endif

	/* host */
	sprintf (buf, "%s:%d", net_ip_str (download->ip), download->port);
	fst_http_request_set_header (request, "Host", buf);

	/* range, http range is inclusive! */
	/* IMPORTANT: use chunk->start + chunk->transmit for starting point, rather non-intuitive */
	sprintf (buf, "bytes=%d-%d", (int)(download->chunk->start + download->chunk->transmit), (int)download->chunk->stop - 1);
	fst_http_request_set_header (request, "Range", buf);

	/* compile and send request */
	packet = fst_packet_create();
	fst_http_request_compile (request, packet);
	fst_http_request_free (request);

	if (fst_packet_send (packet, download->tcpcon) == FALSE)
	{
		download_error_gift (download, FALSE, SOURCE_TIMEOUT, "Request failed");
		fst_packet_free (packet);
		return;
	}

	fst_packet_free (packet);

	/* wait for header */
	input_add (download->tcpcon->fd, (void*)download, INPUT_READ, (InputCallback)download_read_header, FST_DOWNLOAD_HANDSHAKE_TIMEOUT);
}

static void download_read_header (int fd, input_id input, FSTDownload *download)
{
	FSTHttpReply *reply;
	char *p;

	input_remove (input);

	if (net_sock_error (download->tcpcon->fd))
	{
		FST_HEAVY_DBG_2 ("read error while downloading from %s:%d -> removing source", net_ip_str(download->ip), download->port);
		download_error_gift (download, FALSE, SOURCE_TIMEOUT, "Request Failed");
		return;
	}

	/* read data */
	if (fst_packet_recv (download->in_packet, download->tcpcon) == FALSE)
	{
		FST_DBG_2 ("read error while getting header from %s:%d -> aborting", net_ip_str(download->ip), download->port);
		download_error_gift (download, FALSE, SOURCE_TIMEOUT, "Request Failed");
		return;
	}

	reply = fst_http_reply_create ();

	if (fst_http_reply_parse (reply, download->in_packet) == FALSE)
	{	
		fst_http_reply_free (reply);

		if (fst_packet_size (download->in_packet) > 4096)
		{
			FST_WARN ("Didn't get whole http header and received more than 4K, closing connection");
			download_error_gift (download, TRUE, SOURCE_TIMEOUT, "Invalid response");
			return;
		}

		FST_DBG_2 ("didn't get whole header from %s:%d -> waiting for more", net_ip_str(download->ip), download->port);
		/* wait for rest of header */
		input_add (download->tcpcon->fd, (void*)download, INPUT_READ, (InputCallback)download_read_header, FST_DOWNLOAD_HANDSHAKE_TIMEOUT);
		return;
	}

/*
 *	printf ("\nhttp reply from %s: \n", net_ip_str(download->ip));
 *	print_bin_data (download->in_packet->data, fst_packet_size(download->in_packet) - fst_packet_remaining(download->in_packet));
 */

	if (reply->code != 200 && reply->code != 206) /* 206 == partial content */
	{
		FST_HEAVY_DBG_4 ("%s:%d replied with %d (\"%s\") -> aborting", net_ip_str (download->ip), download->port, reply->code, reply->code_str);

		if (reply->code == 503)
			download_error_gift (download, FALSE, SOURCE_QUEUED_REMOTE, "Remotely queued");
		else if (reply->code == 404)
			download_error_gift (download, TRUE, SOURCE_CANCELLED, "File not found");
		else
		{
			download_error_gift (download, TRUE, SOURCE_CANCELLED, "Weird http code");
			FST_DBG_4 ("weird http code from %s:%d: %d (\"%s\") -> aborting", net_ip_str(download->ip), download->port, reply->code, reply->code_str);
		}

		fst_http_reply_free (reply);
		return;
	}

	FST_HEAVY_DBG_4 ("%s:%d replied with %d (\"%s\") -> downloading", net_ip_str(download->ip), download->port, reply->code, reply->code_str);

	/* check that recevied ranges are correct */
	if ( (p = fst_http_reply_get_header (reply, "content-range")))
	{
		int start, stop;
		sscanf (p, "bytes %d-%d", &start, &stop);

		/* longer ranges than requested should be ok since giFT handles this */
		if (start != download->chunk->start + download->chunk->transmit || stop < download->chunk->stop - 1)
		{
			FST_WARN ("Removing source due to range mismatch");
			FST_WARN_2 ("\trequested range: %d-%d", download->chunk->start + download->chunk->transmit, download->chunk->stop - 1);
			FST_WARN_2 ("\treceived range: %d-%d", start, stop);
			if ( (p = fst_http_reply_get_header (reply, "content-length")))
				FST_WARN_1 ("\tcontent-length: %s", p);

			fst_http_reply_free (reply);
			download_error_gift (download, TRUE, SOURCE_CANCELLED, "Range mismatch");
			return;
		}
	}
	else
	{
		FST_WARN ("Server didn't sent content-range header, file may end up corrupted");
/*		
 *		fst_http_reply_free (reply);
 *		download_error_gift (download, TRUE, SOURCE_CANCELLED, "Missing Content-Range");
 *		return;
 */		
	}

	/* update status */
	download->state = DownloadRunning;
	FST_PROTO->source_status (FST_PROTO, download->chunk->source, SOURCE_ACTIVE, "Active");

	/* free reply */
	fst_http_reply_free (reply);

	/* remove header from packet */
	fst_packet_truncate (download->in_packet);

	/* wait for body */
	input_add (download->tcpcon->fd, (void*)download, INPUT_READ, (InputCallback)download_read_body, FST_DOWNLOAD_DATA_TIMEOUT);

	/* write rest of packet to file */
	download_write_gift (download, download->in_packet->read_ptr, fst_packet_remaining (download->in_packet));
}

static void download_read_body (int fd, input_id input, FSTDownload *download)
{
	char *data;
	int len;

	if (net_sock_error (download->tcpcon->fd))
	{
		FST_HEAVY_DBG_2 ("read error while downloading from %s:%d -> aborting", net_ip_str(download->ip), download->port);
		input_remove (input);

		/* this makes giFT call gift_cb_download_stop(), which closes connection and frees download */
		download_error_gift (download, FALSE, SOURCE_CANCELLED, "Download Failed");
		return;
	}
	
	data = malloc (DOWNLOAD_BUF_SIZE);

	if ( (len = tcp_recv (download->tcpcon, data, DOWNLOAD_BUF_SIZE)) <= 0)
	{
		FST_HEAVY_DBG_2 ("download_read_body: tcp_recv() <= 0 for %s:%d", net_ip_str(download->ip), download->port);
		input_remove (input);

		/* this makes giFT call gift_cb_download_stop(), which closes connection and frees download */
		download_error_gift (download, FALSE, SOURCE_CANCELLED, "Download Error");
		return;
	}
	
	/* write data to file through giFT, this calls gift_cb_download_stop() if download is complete */
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
	if (remove_source)
	{
		FST_DBG_1 ("download error (%s), removing source", error);
		FST_PROTO->source_status (FST_PROTO, download->chunk->source, klass, error);
		FST_PROTO->source_abort (FST_PROTO, download->chunk->transfer, download->chunk->source);
	}
	else
	{
		FST_PROTO->source_status (FST_PROTO, download->chunk->source, klass, error);
		download->chunk->udata = NULL;

		/* tell giFT an error occured with this download */
		download_write_gift (download, NULL, 0);
		fst_download_free (download);
	}
}


/* parses url, returns uri which caller frees or NULL on failure */
static char *download_parse_url (char *url, unsigned int *ip, unsigned short *port)
{
	char *tmp, *uri, *ip_str, *port_str;

	if (!url)
		return NULL;

	tmp = uri = strdup (url);

	string_sep (&uri, "://");       /* get rid of this useless crap */

	/* divide the string into two sides */
	if ( (port_str = string_sep (&uri, "/")))
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

#define SWAPU32(x) ( ((((( \
((fst_uint8*)&(x))[0] << 8) | \
((fst_uint8*)&(x))[1]) << 8) | \
((fst_uint8*)&(x))[2]) << 8) | \
((fst_uint8*)&(x))[3])

// returns static base64 encoded string for X-Kazaa-XferUid http header
static char *download_calc_xferuid (char *uri)
{
/*
*	static const unsigned char last_search_hash[32] = {
*		0xbd, 0x48, 0xa4, 0x20, 0x85, 0x4c, 0x2d, 0x30,
*		0xee, 0x07, 0xfd, 0x6c, 0x0f, 0x0b, 0x7c, 0xf7,
*		0x7a, 0xe5, 0x86, 0x41, 0xf8, 0x29, 0x28, 0xbc,
*		0x78, 0xd4, 0xc5, 0x86, 0x6a, 0xd5, 0xde, 0xc7
*	};
*/
	static const unsigned char last_search_hash[32] = {
		0x34, 0x1c, 0x58, 0x01, 0x4d, 0x32, 0xda, 0xeb,
		0xae, 0xe7, 0x32, 0xdc, 0x60, 0xe8, 0x31, 0x76,
		0x1d, 0x47, 0xd7, 0x40, 0x0b, 0x82, 0x4e, 0x41,
		0xe7, 0xef, 0x5c, 0xd1, 0xc0, 0xa7, 0xd0, 0x79
	};

	static const unsigned int VolumeId = 0xB080A125;

	FSTCipher *cipher;
	unsigned int buf[8]; 
	static char base64[40], *base64_ptr;
	unsigned int uri_smhash, smhash;
	unsigned int seed;

	if (uri == NULL)
		return NULL;

	if (*uri == '/')
		uri++;

	uri_smhash = fst_hash_small (uri, strlen(uri), 0xFFFFFFFF);

	memcpy (buf, last_search_hash, 32);
	seed = SWAPU32 (buf[0]);

	/* run through cipher */
	cipher = fst_cipher_create ();

	if (!fst_cipher_init (cipher, seed, 0xB0))
	{
		fst_cipher_free (cipher);
		base64[0] = 0;
		return base64;
	}

	fst_cipher_crypt (cipher, (unsigned char*)(buf+1), 28); 
	fst_cipher_free (cipher);

	seed = SWAPU32 (buf[1]);

	buf[1] = 0;
	smhash = fst_hash_small ( (unsigned char*)(buf+1), 28, 0xFFFFFFFF);

	/* weird */
	if( (seed != smhash) ||
		(SWAPU32(buf[2]) != VolumeId) ||
		(SWAPU32(buf[6]) >= 0x3B9ACA00) ||
		(SWAPU32(buf[7]) >= 0x3B9ACA00) ||
		(SWAPU32(buf[4]) >= 0x3B9ACA00) ||
		(SWAPU32(buf[5]) >= 0x3B9ACA00))
	{
		memset(buf, 0, 32);
	}

	seed = SWAPU32 (buf[3]) - time(NULL);	/* hmm */

	buf[3] = SWAPU32 (seed);
	buf[2] = SWAPU32 (uri_smhash);

	buf[1] = 0;
	smhash = fst_hash_small ( (unsigned char*)(buf+1), 28, 0xFFFFFFFF);
	buf[1] = SWAPU32 (smhash);

	seed = SWAPU32 (buf[3]);
	seed ^= smhash;

	buf[0] = SWAPU32 (seed);

	/* run through cipher */
	cipher = fst_cipher_create();

	if (!fst_cipher_init (cipher, seed, 0xB0))
	{
		fst_cipher_free (cipher);
		base64[0] = 0;
		return base64;
	}

	fst_cipher_crypt (cipher, (unsigned char*)(buf+1), 28); 
	fst_cipher_free (cipher);

	// base64 encode		
	base64_ptr = fst_utils_base64_encode ( (unsigned char*)buf, 32);
	strncpy (base64, base64_ptr, 40);
	base64[39] = '\0';
	free (base64_ptr);
	
	return base64;
}

/*****************************************************************************/
