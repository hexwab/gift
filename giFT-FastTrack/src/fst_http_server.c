/*
 * $Id: fst_http_server.c,v 1.5 2003/12/02 19:50:34 mkern Exp $
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

#include "fst_fasttrack.h"		/* for debug logging */
#include "fst_http_server.h"

/*
#define LOG_HTTP_HEADERS
*/

/*****************************************************************************/

typedef struct
{
	FSTHttpServer *server;	/* server which accepted this connection */

	TCPC *tcpcon;			/* new connection */
	in_addr_t remote_ip;

} ServCon;

/*****************************************************************************/

static void server_accept (int fd, input_id input, FSTHttpServer *server);
static void server_peek (int fd, input_id input, ServCon *servcon);
static void server_request (int fd, input_id input, ServCon *servcon);
static void server_push (int fd, input_id input, ServCon *servcon);
static void server_binary (int fd, input_id input, ServCon *servcon);

/*****************************************************************************/

/* alloc and init server */
FSTHttpServer *fst_http_server_create (in_port_t port,
									   FSTHttpServerRequestCb request_cb,
									   FSTHttpServerPushCb push_cb,
									   FSTHttpServerBinaryCb binary_cb)
{
	FSTHttpServer *server;

	if(! (server = malloc (sizeof (FSTHttpServer))))
		return NULL;

	server->port = port;

	if (! (server->tcpcon = tcp_bind (server->port, FALSE)))
	{
		FST_WARN_1 ("binding to port %d failed", server->port);
		free (server);
		return NULL;
	}

	server->request_cb = request_cb;
	server->push_cb = push_cb;
	server->binary_cb = binary_cb;

	server->banlist_filter = config_get_int (FST_PLUGIN->conf,
	                                         "main/banlist_filter=0");

	/* wait for incomming connection */
	server->input = input_add (server->tcpcon->fd, (void *)server, INPUT_READ,
							   (InputCallback)server_accept, 0);

	return server;
}

/* free server, close listening port */
void fst_http_server_free (FSTHttpServer *server)
{
	if (!server)
		return;

	input_remove (server->input);
	tcp_close_null (&server->tcpcon);

	free (server);
}

/*****************************************************************************/

static void server_accept (int fd, input_id input, FSTHttpServer *server)
{
	ServCon *servcon;

	if (net_sock_error (fd))
	{
		/* cannot happen */
		FST_ERR_1 ("net_sock_error for fd listening on port %d",
				   server->tcpcon->port);
		return;
	}

	if(! (servcon = malloc (sizeof (ServCon))))
		return;

	if (! (servcon->tcpcon = tcp_accept (server->tcpcon, FALSE)))
	{
		FST_WARN_1 ("accepting socket from port %d failed",
					server->tcpcon->port);
		free (servcon);
		return;
	}

	servcon->server = server;
	servcon->remote_ip = net_peer (servcon->tcpcon->fd);

	/* deny requests from banned ips */
	if (server->banlist_filter &&
        fst_ipset_contains (FST_PLUGIN->banlist, servcon->remote_ip))
	{
		FST_DBG_1 ("denied incoming connection from %s based on banlist",
				   net_ip_str (servcon->remote_ip));

		tcp_close (servcon->tcpcon);
		free (servcon);
		return;
	}
	else
	{
		FST_HEAVY_DBG_1 ("accepted incoming connection from %s",
		                 net_ip_str (servcon->remote_ip));
	}

	/* wait for data */
	input_add (servcon->tcpcon->fd, (void *)servcon, INPUT_READ,
			   (InputCallback)server_peek, HTSV_REQUEST_TIMEOUT);
}

static void server_peek (int fd, input_id input, ServCon *servcon)
{
	unsigned char buf[5];
	int len;

	input_remove (input);

	if (net_sock_error (fd))
	{
		/* remote closed, possibly timeout */
		FST_DBG_1 ("connection from %s closed without receiving any data",
				   net_ip_str (servcon->remote_ip));

		tcp_close_null (&servcon->tcpcon);
		free (servcon);
		return;
	}

	/* we use the first 4 bytes to detrmine the type of connection.
	 * for simplicity we assume that those 4 bytes are available at once.
	 */
	if ((len = tcp_peek (servcon->tcpcon, buf, 4)) != 4)
	{
		FST_DBG_1 ("received less than 4 bytes from %s, closing connection",
				   net_ip_str (servcon->remote_ip));

		tcp_close_null (&servcon->tcpcon);
		free (servcon);
		return;
	}

	buf[4] = 0;

	if (!strcmp (buf, "GET "))
	{
		FST_HEAVY_DBG_2 ("connection from %s is a http request [%s]",
		                 net_ip_str (servcon->remote_ip), buf);
		input_add (servcon->tcpcon->fd, (void *)servcon, INPUT_READ,
				   (InputCallback)server_request, HTSV_REQUEST_TIMEOUT);
	}
	else if (!strcmp (buf, "GIVE"))
	{
		FST_HEAVY_DBG_2 ("connection from %s is a push reply [%s]",
		                 net_ip_str (servcon->remote_ip), buf);
		input_add (servcon->tcpcon->fd, (void *)servcon, INPUT_READ,
				   (InputCallback)server_push, HTSV_REQUEST_TIMEOUT);
	}
	else 
	{
		FST_DBG_5 ("connection from %s is binary [%02X%02X%02X%02X]",
				   net_ip_str (servcon->remote_ip), buf[0], buf[1],
				   buf[2], buf[3]);
		input_add (servcon->tcpcon->fd, (void *)servcon, INPUT_READ,
				   (InputCallback)server_binary, HTSV_REQUEST_TIMEOUT);
	}
}

/*****************************************************************************/

static void server_request (int fd, input_id input, ServCon *servcon)
{
	FDBuf *buf;
	int len;
	char *header_str;
	FSTHttpHeader *request;

	input_remove (input);

	if (net_sock_error (fd))
	{
		/* remote closed, possibly timeout */
		FST_DBG_1 ("net_sock_error for connection from %s",
				   net_ip_str (servcon->remote_ip));

		tcp_close_null (&servcon->tcpcon);
		free (servcon);
		return;
	}

	buf = tcp_readbuf (servcon->tcpcon);

	if ((len = fdbuf_delim (buf, "\r\n\r\n")) < 0)
	{
		FST_DBG_1 ("fdbuf_delim() < 0 for connection from %s",
				   net_ip_str (servcon->remote_ip));

		tcp_close_null (&servcon->tcpcon);
		free (servcon);
		return;
	}

	if (len > 0)
	{
		if (len > HTSV_MAX_REQUEST_LEN)
		{
			/* invalid data, close connection */
			FST_DBG_2 ("got more than %d bytes from from %s but no sentinel, closing connection",
					   HTSV_MAX_REQUEST_LEN, net_ip_str (servcon->remote_ip));

			tcp_close_null (&servcon->tcpcon);
			free (servcon);
			return;
		}

		/* wait for more data*/
		FST_HEAVY_DBG_2 ("got %d bytes from %s, waiting for more",
						 len, net_ip_str (servcon->remote_ip));
		input_add (servcon->tcpcon->fd, (void *)servcon, INPUT_READ,
				   (InputCallback)server_request, HTSV_REQUEST_TIMEOUT);
		return;
	}

	header_str = fdbuf_data (buf, &len);

	/* parse header */
	if (! (request = fst_http_header_parse (header_str, &len)))
	{
		FST_DBG_1 ("parsing header failed for connection from %s, closing connection",
				   net_ip_str (servcon->remote_ip));

		fdbuf_release (buf);
		tcp_close_null (&servcon->tcpcon);
		free (servcon);
		return;
	}

#ifdef LOG_HTTP_HEADERS
	header_str[len] = 0; /* chops off last '\n', we just use it for debugging */
	FST_HEAVY_DBG_2 ("http request received from %s:\r\n%s",
					 net_ip_str(servcon->remote_ip), header_str);
#endif

	fdbuf_release (buf);

	/* raise callback */
	if (!servcon->server->request_cb ||
		!servcon->server->request_cb (servcon->server, servcon->tcpcon, request))
	{
		FST_DBG_1 ("Connection from %s closed on callback's request",
				   net_ip_str (servcon->remote_ip));
		fst_http_header_free (request);
		tcp_close_null (&servcon->tcpcon);
	}

	free (servcon);
}

static void server_push (int fd, input_id input, ServCon *servcon)
{
	FDBuf *buf;
	char *give_str, *p;
	int len;
	unsigned int push_id; 

	input_remove (input);

	if (net_sock_error (fd))
	{
		/* remote closed, possibly timeout */
		FST_DBG_1 ("net_sock_error for connection from %s",
				   net_ip_str (servcon->remote_ip));

		tcp_close_null (&servcon->tcpcon);
		free (servcon);
		return;
	}

	buf = tcp_readbuf (servcon->tcpcon);

	if ((len = fdbuf_delim (buf, "\r\n")) < 0)
	{
		FST_DBG_1 ("fdbuf_delim() < 0 for connection from %s",
				   net_ip_str (servcon->remote_ip));

		tcp_close_null (&servcon->tcpcon);
		free (servcon);
		return;
	}

	if (len > 0)
	{
		if (len > HTSV_MAX_REQUEST_LEN)
		{
			/* invalid data, close connection */
			FST_DBG_2 ("got more than %d bytes from from %s but no sentinel, closing connection",
					   HTSV_MAX_REQUEST_LEN, net_ip_str (servcon->remote_ip));

			tcp_close_null (&servcon->tcpcon);
			free (servcon);
			return;
		}

		/* wait for more data*/
		FST_HEAVY_DBG_2 ("got %d bytes from %s, waiting for more",
						 len, net_ip_str (servcon->remote_ip));
		input_add (servcon->tcpcon->fd, (void *)servcon, INPUT_READ,
				   (InputCallback)server_request, HTSV_REQUEST_TIMEOUT);
		return;
	}

	give_str = fdbuf_data (buf, &len);

#ifdef LOG_HTTP_HEADERS
	give_str[len] = 0; /* chops off last '\n', we just use it for debugging */
	FST_HEAVY_DBG_2 ("push reply received from %s:\r\n%s",
					 net_ip_str(servcon->remote_ip), give_str);
#endif

	/* parse push reply */
	p = give_str;
	string_sep (&p, " "); /* skip "GIVE " */
	push_id = ATOL (p);

	fdbuf_release (buf);

	/* raise callback */
	if (!servcon->server->push_cb ||
		!servcon->server->push_cb (servcon->server, servcon->tcpcon, push_id))
	{
		FST_DBG_1 ("Connection from %s closed on callback's request",
				   net_ip_str (servcon->remote_ip));
		tcp_close_null (&servcon->tcpcon);
	}

	free (servcon);
}

static void server_binary (int fd, input_id input, ServCon *servcon)
{
	input_remove (input);

	if (net_sock_error (fd))
	{
		/* remote closed, possibly timeout */
		FST_DBG_1 ("net_sock_error for connection from %s",
				   net_ip_str (servcon->remote_ip));

		tcp_close_null (&servcon->tcpcon);
		free (servcon);
		return;
	}

	/* raise callback */
	if (!servcon->server->binary_cb ||
		!servcon->server->binary_cb (servcon->server, servcon->tcpcon))
	{
		FST_DBG_1 ("Connection from %s closed on callback's request",
				   net_ip_str (servcon->remote_ip));
		tcp_close_null (&servcon->tcpcon);
	}

	free (servcon);
}

/*****************************************************************************/
