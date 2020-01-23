/*
 * http.c
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#include "openft.h"

#include "nb.h"
#include "http.h"
#include "mime.h"
#include "html.h"

#include "file.h"
#include "parse.h"

/*****************************************************************************/

#define HTTP_USER_AGENT PACKAGE " " VERSION

/* TODO - a dropped/incomplete firewalled download will leak from this
 * structure */
static Dataset *indirect_downloads = NULL;

/* log_format_time */
static const char *month_tab =
	"Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec ";

/*****************************************************************************/

/* i didnt write this, dont blame me */
static char *access_log_format_time (time_t log_time)
{
	static char  buf[30];
	char        *p;
	struct tm   *t;
	unsigned int a;
	int          time_offset = 0;

	t = gmtime (&log_time);

	p = buf + 29;

	*p-- = '\0';
	*p-- = ' ';
	*p-- = ']';
	a = abs(time_offset / 60);
	*p-- = '0' + a % 10;
	a /= 10;
	*p-- = '0' + a % 6;
	a /= 6;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = (time_offset > 0) ? '-' : '+';
	*p-- = ' ';

	a = t->tm_sec;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ':';
	a = t->tm_min;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ':';
	a = t->tm_hour;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ':';
	a = 1900 + t->tm_year;
	while (a)
	{
		*p-- = '0' + a % 10;
		a /= 10;
	}
	/* p points to an unused spot */
	*p-- = '/';
	p -= 2;
	memcpy(p--, month_tab + 4 * (t->tm_mon), 3);
	*p-- = '/';
	a = t->tm_mday;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p = '[';

	/* should be same as returning buf */
	return p;
}

/* apache-style access.log */
static void access_log (OpenFT_Transfer *xfer, int status_code,
                        size_t content_length)
{
	FILE *f;
	char *referer;
	char *user_agent;

	if (!xfer || !xfer->filename)
		return;

	if (!strncmp (xfer->filename, DATA_DIR, strlen (DATA_DIR)))
		return;

	referer    = dataset_lookup (xfer->header, "Referer");
	user_agent = dataset_lookup (xfer->header, "User-Agent");

	if (!referer)
		referer = "-";

	if (!user_agent)
		user_agent = "-";

	if (!(f = fopen (gift_conf_path ("access.log"), "a")))
	{
		GIFT_ERROR (("failed to open ~/.giFT/access.log"));
		return;
	}

	fprintf (f, "%s - - %s\"%s %s HTTP/1.0\" %i %i \"%s\" \"%s\" \n",
	         net_ip_str (xfer->ip), access_log_format_time (time (NULL)),
	         (char *) dataset_lookup (xfer->header, "rtype"),
	         xfer->filename, status_code, content_length, referer,
	         user_agent);

	fclose (f);
}

/*****************************************************************************/

static void header_free (Dataset *dataset)
{
	hash_table_destroy_free (dataset);
}

/*****************************************************************************/

static void transfer_set_file (OpenFT_Transfer *xfer, char *filename)
{
	/* it can happen :)  if theyre requesting a special file (ie /OpenFT/)...
	 * just trust me */
	if (xfer->filename == filename)
		return;

	if (xfer->filename || xfer->encoded)
	{
		free (xfer->filename);
		free (xfer->encoded);
	}

	xfer->filename = STRDUP (filename);
	xfer->encoded  = url_encode (filename);
}

/*****************************************************************************/

OpenFT_Transfer *http_transfer_new (OpenFT_TransferCB cb,
                                    unsigned long ip, unsigned short port,
                                    char *filename, size_t start, size_t stop)
{
	OpenFT_Transfer *xfer;

	xfer = malloc (sizeof (OpenFT_Transfer));
	memset (xfer, 0, sizeof (OpenFT_Transfer));

	xfer->callback = cb;

	xfer->ip       = ip;
	xfer->port     = port;

	transfer_set_file (xfer, filename);

	xfer->range_start = start;
	xfer->range_stop  = stop;

#if 0
	/* assume that this is not a file transfer until later proven to be */
	xfer->file_transfer = FALSE;
#endif

#if 0
	if (!active_chunks)
		active_chunks = hash_table_new ();
#endif

	return xfer;
}

void http_transfer_free (OpenFT_Transfer *xfer)
{
#if 0
	/* this chunk is no longer active */
	hash_table_remove (active_chunks, (unsigned long) transfer->chunk);
#endif

	free (xfer->filename);
	free (xfer->encoded);

	header_free (xfer->header);

	/* for uploads, close the file handle */
	if (xfer->f)
		fclose (xfer->f);

	free (xfer);
}

/*****************************************************************************/

/* parse a string resembling "Key1: Value\r\nKeyn: Value\r\n" into a dataset */
static void header_parse (Dataset *dataset, char *http_request)
{
	char *token;

	while ((token = string_sep_set (&http_request, "\r\n")))
	{
		char *key;

		trim_whitespace (token);

		key = string_sep (&token, ": ");
		if (!key || !token)
			continue;

		dataset_insert (dataset, key, STRDUP (token));
	}
}

/* parse the HTTP request off of the header for http_header_parse */
static Dataset *request_parse (char *http_request)
{
	Dataset *dataset = NULL;
	char    *request;
	char    *rtype;

	if (!http_request)
		return NULL;

	/* data is expected to be in the format:
	 *
	 *   {GET|PUT|PUSH|HEAD} /blabla[ crap]\r\n
	 */

	rtype   = string_sep     (&http_request, " ");     /* GET|PUT|HEAD */
	request = string_sep_set (&http_request, " \r\n"); /* /blabla */

	if (!request)
		return NULL;

	/* TODO - verify request */

	request = url_decode (request);

	dataset_insert (dataset, "rtype",    STRDUP (rtype));
	dataset_insert (dataset, "request",  request);

	header_parse (dataset, http_request);

	return dataset;
}

/* parse the HTTP reply from the header */
static Dataset *response_parse (char *http_response)
{
	Dataset *dataset = NULL;
	char    *response;

	response = string_sep_set (&http_response, " \r\n");

	if (!response)
		return NULL;

	dataset_insert (dataset, "response", STRDUP (response));

	header_parse (dataset, http_response);

	return dataset;
}

/*****************************************************************************/

/* parse ?key=value&keyn=value&... into a dataset */
static Dataset *query_parse (char *request)
{
	Dataset *dataset = NULL;
	char *token;

	while ((token = string_sep (&request, "&")))
	{
		char *key;

		key = string_sep (&token, "=");
		if (!key || !token)
			continue;

		dataset_insert (dataset, key, STRDUP (token));
	}

	return dataset;
}

/* handle cgi queries */
char *http_handle_cgiquery (Connection *c, char *request)
{
	unsigned short  listen_port;
	unsigned long   request_host;
	char           *request_file;
	Dataset        *dataset;

	TRACE_SOCK (("QUERY: %s", request));

	/* parse it -- this should be another func */
	dataset = query_parse (request);

	/* only thing we currently use this code for is firewalled uploading */
	listen_port  = ATOI      (dataset_lookup (dataset, "listen_port"));
	request_host = (dataset_lookup (dataset, "request_host") ?
					inet_addr (dataset_lookup (dataset, "request_host")) : 0);
	request_file = dataset_lookup (dataset, "request_file");

	if (request_host && request_file)
	{
		char *decoded = url_decode (request_file);

		ft_packet_send_indirect (request_host, FT_PUSH_REQUEST, "%lu%hu%s",
		                       (c ? ntohl (inet_addr (net_peer_ip (c->fd))) : 0),
		                       listen_port, decoded);

		free (decoded);
	}

	header_free (dataset);

	return html_page_redirect (gift_conf_path ("OpenFT/redirect.html"));
}

/*****************************************************************************/

static int http_connection_close (Connection *c, Chunk *chunk,
                                  OpenFT_Transfer *xfer)
{
	if (c)
	{
		/* cleanup with the event loop */
		input_remove (c);
		net_close (c->fd);
	}

	if (chunk)
	{
		/* indirect connections will want to re-use the same chunk structure,
		 * so rather than free it here, set the data element to NULL to be
		 * reassigned later */
		if (xfer->indirect)
			chunk->data = NULL;
		else
		{
			dataset_remove (indirect_downloads, xfer->filename);

			/* notify the transfer callback that we have terminated this
			 * connection */
			if (xfer->callback)
				(*xfer->callback) (chunk, NULL, 0);

			/* WARNING: chunk is free'd in xfer->callback! */
		}
	}

	/* free any data associated */
	http_transfer_free (xfer);

	connection_destroy (c);

	return FALSE;
}

/*****************************************************************************/

/* callback to read from the desired file and write to the requesting
 * client */
static int connection_deliver (Protocol *p, Connection *c)
{
	Chunk           *chunk = c->data;
	OpenFT_Transfer *xfer  = OPENFT_TRANSFER (chunk);
	char             buf[RW_BUFFER + 1];
	size_t           send_len;
	int              n;

	/* TODO - chunk should be opening files! */
	send_len = fread (buf, sizeof (char), sizeof (buf) - 1, xfer->f);

	/* If an error occurs, or the end-of-file is reached, the return value
	 * is a short item count (or zero).  fread (3) ??? */
	if (send_len > sizeof (buf))
		send_len = 0;

	if (send_len > (xfer->range_stop - chunk->transmit))
		send_len = xfer->range_stop - chunk->transmit;

	if (send_len)
		n = send (c->fd, buf, send_len, 0);

	if (send_len == 0 || n <= 0)
	{
		http_connection_close (c, chunk, xfer);

		return FALSE;
	}

#if 0
	if (!chunk)
	{
		char *file;

		if ((file = strrchr (transfer->filename, '/')))
			file++;

		transfer->chunk = malloc (sizeof (Chunk));
		memset (transfer->chunk, 0, sizeof (Chunk));

		/* this is a stupid design.  HTTPTransfer -> Chunk -> Transfer doesnt
		 * make as much sense as Chunk -> Transfer -> HTTPTransfer or
		 * something...sigh...TODO */
		transfer->chunk->transfer =
		    upload_new (p, net_peer_ip (c->fd), file, transfer->filename,
		                transfer->range_start, transfer->range_stop);
	}
#endif

	(*xfer->callback) (chunk, buf, send_len);

	return TRUE;
}

/*****************************************************************************/

/* send the appropriate HTTP reply header
 * NOTE: this code is used for both push and standard get requests */
static int send_reply_header (OpenFT_Transfer *xfer, int verify_share,
                              size_t start, size_t stop, size_t length,
                              int is_push, int nodepage)
{
	char  http[4096];
	char *range;
	char *true_path;
	char *content_type = "";
	int	  http_status_code = 200;

	/* check whether the file is among those shared */
	if (verify_share)
	{
		if (!nodepage)
		{
			if (!(true_path = openft_share_local_verify (xfer->filename)))
				http_status_code = 404;
			else
			{
				/* TODO - this will be cleaned up in the future */
				FileShare *file = openft_share_local_find (xfer->filename);
				xfer->hash      = (file ? STRDUP (file->md5) : NULL);

				transfer_set_file (xfer, true_path);
			}
		}

		if (http_status_code != 404)
		{
			xfer->f = mime_open (xfer->filename, "rb",
			                     &content_type, &length);

			/* if we can't open file its a 404 */
			if (!xfer->f)
				http_status_code = 404;
		}

		/* log this transfer */
		access_log (xfer, http_status_code, length);
	}

	if (http_status_code == 404)
	{
		net_send (xfer->c->fd, "HTTP/1.0 404 Not Found\r\n\r\n");
		return FALSE;
	}

	/* ok, actually construct the reply now ... TODO - this code sucks */

	/* if we aren't pushing the remote host will reliably give us the range
	 * information that they wish to use */
	if (!is_push)
	{
		char sep_chr;

		/* Content-Range: is used for PUSH request replies... if we
		 * read it here we can guarantee that the individual function doesn't
		 * need to handle it...shouldn't break anything either */
		if (!(range = dataset_lookup (xfer->header, "Range")))
			range = dataset_lookup (xfer->header, "Content-Range");

		/* this is wrong.  more correct HTTP support is to follow */
		if (range)
			sscanf (range, "bytes%c%i-%i", &sep_chr, &start, &stop);
	}

	if (start || stop)
	{
		if (!stop)
			stop = length;

		/* some very derranged conditions can allow stop to be 0 at this point,
		 * but that means the remote node fucked up so we will let some other
		 * subsystem catch the problem */

		sprintf (http,
		         "HTTP/1.0 206 Partial Content\r\n"
		         "Content-Range: bytes %i-%i/%i\r\n"
		         "Content-Length: %i\r\n"
		         "Content-Type: %s\r\n\r\n",
				 start, stop, length, stop - start, content_type);

		/* seek to the range start */
		if (!is_push)
			fseek (xfer->f, start, SEEK_SET);

		/* this is really redundant for is_push, but oh well */
		xfer->range_start = start;
		xfer->range_stop  = stop;

		/* length = stop - start; */
	}
	else
	{
		sprintf (http,
		         "HTTP/1.1 200 OK\r\n"
		         "Content-Length: %i\r\n"
		         "Content-Type: %s\r\n\r\n",
		         length, content_type);

		xfer->range_start = 0;
		xfer->range_stop  = length;
	}

	send (xfer->c->fd, http, strlen (http), 0);

	return TRUE;
}

/*****************************************************************************/
/* TODO clean this mess up */

static int read_body (Protocol *p, Connection *c)
{
	Chunk           *chunk = c->data;
	OpenFT_Transfer *xfer  = OPENFT_TRANSFER (chunk);
	char             buf[RW_BUFFER];
	int              n;

	if ((n = recv (c->fd, buf, sizeof (buf) - 1, 0)) <= 0)
	{
		TRACE_SOCK (("remotely closed"));
		http_connection_close (c, chunk, xfer);
		return FALSE;
	}

	/* indirect connections should not report data back to the daemon, it's
	 * probably incorrect ... wait for the connection to become direct */
	if (!xfer->indirect)
	{
		(*xfer->callback) (chunk, buf, n);
	}

	return TRUE;
}

/* receive the HTTP reply from the socket */
static int read_response (Protocol *p, Connection *c)
{
	Chunk           *chunk = c->data;
	OpenFT_Transfer *xfer  = OPENFT_TRANSFER (chunk);
	NBRead          *nb;
	int              n;

	TRACE_SOCK (("entered"));

	nb = nb_active (c->fd);

	n = nb_read (nb, c->fd, 0, "\r\n\r\n");
	if (n <= 0)
	{
		nb_finish (nb);
		http_connection_close (c, chunk, xfer);
		return FALSE;
	}

	if (!nb->term)
		return FALSE;

	xfer->header = response_parse (nb->data);

	nb_finish (nb);

	if (!xfer->header)
	{
		TRACE_SOCK (("invalid http header"));
		http_connection_close (c, chunk, xfer);
		return FALSE;
	}

	input_remove (c);
	input_add (p, c, INPUT_READ, (InputCallback) read_body, FALSE);

	return TRUE;
}

/*****************************************************************************/

/* handles incoming request processing */
static int incoming_connection (Protocol *p, Connection *c)
{
	Transfer        *transfer;
	OpenFT_Transfer *xfer = c->data; /* TEMPORARY! */
	char             buf[RW_BUFFER];
	char            *nodepage = NULL;
	char            *rtype;
	int              n;

	/* clear the read queue */
	if ((n = recv (c->fd, buf, sizeof (buf) - 1, 0)) <= 0)
	{
		http_connection_close (c, NULL, xfer);
		return FALSE;
	}

	buf[n] = 0;

	/* parse */
	if (!(xfer->header = request_parse (buf)))
	{
		TRACE_SOCK (("invalid http header"));
		http_connection_close (c, NULL, xfer);
		return FALSE;
	}

	/* associate the file data with the request */
	transfer_set_file (xfer, dataset_lookup (xfer->header, "request"));

	/* process the request type */
	rtype = dataset_lookup (xfer->header, "rtype");

	/* ok, this is a non-standard OpenFT HTTP session, send the ack then come
	 * back to this connection and read the response */
	if (!strcasecmp (rtype, "PUSH"))
	{
		Chunk    *chunk;

		TRACE_SOCK (("push (%s)", xfer->filename));

		/* locate the saved chunk structure */
		if (!(chunk = dataset_lookup (indirect_downloads, xfer->filename)))
		{
			TRACE_SOCK (("unrequested file %s...rejecting!", xfer->filename));
			http_connection_close (c, NULL, xfer);
			return FALSE;
		}

		dataset_remove (indirect_downloads, xfer->filename);

		/* close the original connection, its useless now, and we need to
		 * borrow its chunk buffer */
		if (chunk->data)
		{
			OpenFT_Transfer *dummy_xfer = OPENFT_TRANSFER (chunk);
			http_connection_close (dummy_xfer->c, NULL, dummy_xfer);
		}

		xfer->indirect = FALSE;

		/* wrap the chunk/connection associations for consistency */
		c->data     = chunk;
		chunk->data = xfer;

		GIFT_DEBUG (("directional change: push -> ft_download (%lu-%lu, %lu)",
					 chunk->start, chunk->stop, chunk->transmit));
		xfer->callback = ft_download;

		/* this is a bit redundant, i think ... send_reply_header's supposed
		 * to do this */
		xfer->range_start = chunk->start;
		xfer->range_stop  = chunk->stop;

		/* they just notified us that they are going to push the selected file
		 * to us, we now need to notify them of the range request we want from
		 * the file */
		send_reply_header (xfer, FALSE, chunk->start, chunk->stop,
		                   chunk->stop - chunk->start, TRUE, FALSE);

		input_remove (c);
		input_add (p, c, INPUT_READ, (InputCallback) read_response, FALSE);

		return TRUE;
	}

	/* no file was requested, send the nodepage */
	if (!xfer->filename[1])
	{
		nodepage = html_page_index (gift_conf_path ("OpenFT/nodepage.html"));
		transfer_set_file (xfer, nodepage);
	}
	/* nodepage data */
	else if (!strncmp (xfer->filename, "/OpenFT/", 8))
	{
		char *true_path;

		true_path = malloc (strlen (xfer->filename) + strlen (DATA_DIR) + 10);
		sprintf (true_path, "%s%s", DATA_DIR, xfer->filename);

		transfer_set_file (xfer, true_path);

		free (true_path);
	}
	/* CGI query */
	else if (xfer->filename[1] == '?')
	{
		nodepage = http_handle_cgiquery (c, xfer->filename + 2);
		transfer_set_file (xfer, nodepage);
	}

	/* NOTE: regular file downloads will fall through to this point...share
	 * verification will occur later */

	/* respond */
	if (!send_reply_header (xfer, TRUE, 0, 0, 0, FALSE,
	                        (nodepage ? TRUE : FALSE)))
	{
		/* something didn't check out right, set to an invalid file share and
		 * fail later */
		transfer_set_file (xfer, NULL);
	}

	/* no file was meant to be written, die */
	if (!xfer->filename)
	{
		http_connection_close (c, NULL, xfer);
		return FALSE;
	}

	/* we now know this is an upload, register it so that we can get the
	 * necessary structural data */
	transfer = upload_new (protocol_find ("OpenFT"), net_peer_ip (c->fd),
	                       xfer->hash,
	                       file_basename (xfer->filename),
	                       xfer->filename,
	                       xfer->range_start, xfer->range_stop);

	/* wrap the chunk/connection associations for consistency */
	c->data = transfer->chunks->data;
	((Chunk *)c->data)->data = xfer;

	/* handle the push'd data */
	input_remove (c);
	input_add (p, c, INPUT_WRITE, (InputCallback) connection_deliver, FALSE);

	return TRUE;
}

/*****************************************************************************/

/* incoming connections callback */
void http_handle_incoming (Protocol *p, Connection *c)
{
	OpenFT_Transfer *xfer;
	Connection      *new_c;

	new_c       = connection_new (p);
	new_c->fd   = net_accept (c->fd);

	/* this will be changed as soon as we identify the request.  the default
	 * chunk encapsulation will occur */
	xfer = http_transfer_new (ft_upload, inet_addr (net_peer_ip (new_c->fd)),
	                          0, NULL, 0, 0);

	new_c->data = xfer;
	xfer->c     = new_c;

	input_add (p, new_c, INPUT_READ,
	           (InputCallback) incoming_connection, TRUE);
}

/*****************************************************************************/

/* make an HTTP request */
static int send_request (int fd, char *rtype, char *request, char *extra)
{
	char  *req;
	size_t req_len = 0;
	int    ret;

	if (!rtype || !request)
		return -1;

	/* just to be safe */
	req = malloc (512 + (extra ? strlen (extra) : 0) + strlen (request));

	/* write the default header */
	req_len += sprintf (req, "%s %s%s HTTP/1.1\r\nUser-Agent: %s\r\n",
						rtype, ((*request != '/') ? "/" : ""), request,
						HTTP_USER_AGENT);

	/* append any arbitrary user data */
	if (extra)
		req_len += sprintf (req + req_len, "%s\r\n", extra);

	/* extra \r\n */
	req_len += sprintf (req + req_len, "\r\n");

	ret = send (fd, req, req_len, 0);

	free (req);

	return ret;
}

/*****************************************************************************/

static int push_ack (Protocol *p, Connection *c)
{
	Chunk           *chunk = c->data;
	OpenFT_Transfer *xfer  = OPENFT_TRANSFER (chunk);
	char             buf[RW_BUFFER];
	int              n;

	TRACE_SOCK (("entered"));

	if ((n = recv (c->fd, buf, sizeof (buf) - 1, 0)) <= 0)
	{
		http_connection_close (c, chunk, xfer);
		return FALSE;
	}

	buf[n] = 0;

	if (!(xfer->header = response_parse (buf)))
	{
		TRACE_SOCK (("invalid http header"));
		http_connection_close (c, chunk, xfer);
		return FALSE;
	}

	/* TODO - evaluate response */

	/* write the header, this will also handle setting the range request
	 * stuff */
	if (!send_reply_header (xfer, TRUE, 0, 0, 0, FALSE, FALSE))
	{
		http_connection_close (c, chunk, xfer);
		return FALSE;
	}

	TRACE_SOCK (("pushing %lu-%lu", xfer->range_start, xfer->range_stop));

	input_remove (c);
	input_add (p, c, INPUT_WRITE, (InputCallback) connection_deliver, FALSE);

	return TRUE;
}

static int push_verify_source (Protocol *p, Connection *c)
{
	Chunk           *chunk = c->data;
	OpenFT_Transfer *xfer  = OPENFT_TRANSFER (chunk);

	TRACE_SOCK (("entered"));

	if (net_sock_error (c->fd))
	{
		http_connection_close (c, chunk, xfer);
		return FALSE;
	}

	/* send the PUSH / request...this is non-standard, I think... I just made
	 * it up */
	send_request (c->fd, "PUSH", xfer->encoded, NULL);

#if 0
	/* now that we've sent the request, set the true file locally */
	if (!(true_path = openft_share_local_verify (xfer->filename)))
	{
		TRACE_SOCK (("%s not shared", xfer->filename));
		http_connection_close (c, chunk, xfer);
	}

	transfer_set_file (xfer, true_path);
#endif

	input_remove (c);
	input_add (p, c, INPUT_READ, (InputCallback) push_ack, FALSE);

	return TRUE;
}

/* connect outgoing and "push" the requested file */
int http_push_file (Chunk *chunk)
{
	OpenFT_Transfer *xfer = OPENFT_TRANSFER (chunk);
	Connection      *c;

	TRACE_FUNC ();

#if 0
	xfer->file_transfer = TRUE;
#endif

	TRACE (("connecting to %s:%hu",
	        net_ip_str (xfer->ip), xfer->port));

	/* make the outgoing connection structure */
	c     = connection_new (NULL);
	c->fd = net_connect (net_ip_str (xfer->ip), xfer->port);

	/* circular lookup again */
	c->data = chunk;
	xfer->c = c;

	input_add (NULL, c, INPUT_WRITE,
	           (InputCallback) push_verify_source, TRUE);

#if 0
	hash_table_insert (active_chunks, (unsigned long) transfer->chunk, c);
#endif

	return TRUE;
}

/*****************************************************************************/

static int pull_verify_source (Protocol *p, Connection *c)
{
	Chunk           *chunk = c->data;
	OpenFT_Transfer *xfer  = OPENFT_TRANSFER (chunk);
	char            *range = NULL;

	TRACE_SOCK (("entered"));

	if (net_sock_error (c->fd))
	{
		http_connection_close (c, chunk, xfer);
		return FALSE;
	}

	/* range_start and stop will both be 0 if we mean to request the entire
	 * file */
	if (xfer->range_stop)
	{
		range = malloc (128);
		sprintf (range, "Range: bytes=%lu-%lu\r\n",
				 xfer->range_start, xfer->range_stop);
	}

	/* send the GET / request */
	send_request (c->fd, "GET", xfer->encoded, range);

	free (range);

	/* recycle this input */
	input_remove (c);
	input_add (p, c, INPUT_READ, (InputCallback) read_response, FALSE);

	return TRUE;
}

int http_pull_file (Chunk *chunk, char *source, int indirect)
{
	OpenFT_Transfer *xfer = OPENFT_TRANSFER (chunk);
	Connection *c;

#if 0
	xfer->file_transfer = TRUE;
#endif

	if (indirect)
	{
		char *reqfile;
		char *decode;

		xfer->indirect = TRUE;

		/* TODO - this is just plain wrong! */
		if ((reqfile = strstr (xfer->filename, "request_file=")))
			reqfile += strlen ("request_file=");

		decode = url_decode (reqfile);

		TRACE (("indirect_downloads: %s", decode));
		dataset_insert (indirect_downloads, decode, chunk);

		free (decode);
	}

	TRACE (("connecting to %s:%hu",
	        net_ip_str (xfer->ip), xfer->port));

	/* dont try to deliver this request to yourself via HTTP, just do it
	 * directly */
	if (!xfer->ip)
	{
		char *query = STRDUP (source);

		http_handle_cgiquery (NULL, query);

		free (query);

		return TRUE;
	}

	/* make the outgoing connection */
	c     = connection_new (NULL);
	c->fd = net_connect (net_ip_str (xfer->ip), xfer->port);

	/* circular data lookup */
	c->data = chunk;
	xfer->c = c;

	/* wait to submit the header */
	input_add (NULL, c, INPUT_WRITE,
	           (InputCallback) pull_verify_source, TRUE);

#if 0
	hash_table_insert (active_chunks, (unsigned long) transfer->chunk, c);
#endif

	return TRUE;
}

/*****************************************************************************/

void http_cancel (Chunk *chunk)
{
	OpenFT_Transfer *xfer = OPENFT_TRANSFER (chunk);

	if (!xfer)
		return;

	TRACE_FUNC ();

#if 0
	/* TODO - use a unique id */
	if (!(http_c = hash_table_lookup (active_chunks, (unsigned long) chunk)))
		return;
#endif

	/* callback should NOT be made, we were instructed to remove this data */
	xfer->callback = NULL;
	http_connection_close (xfer->c, chunk, xfer);
}
