/*
 * $Id: as_http_server.c,v 1.9 2004/12/16 23:27:44 hex Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*
#define LOG_HTTP_HEADERS
*/

/*****************************************************************************/

typedef struct
{
	ASHttpServer *server;	/* server which accepted this connection */

	TCPC *tcpcon;			/* new connection */
	in_addr_t remote_ip;

	String *buf;            /* http request buffer */
	List   *link;
	input_id input;
} ServCon;

/*****************************************************************************/

static void server_accept (int fd, input_id input, ASHttpServer *server);
static void server_peek (int fd, input_id input, ServCon *servcon);
static void server_request (int fd, input_id input, ServCon *servcon);
static void server_push (int fd, input_id input, ServCon *servcon);
static void server_binary (int fd, input_id input, ServCon *servcon);

/*****************************************************************************/

static ServCon *servcon_new (ASHttpServer *server, TCPC *c)
{
	ServCon *servcon;

	if (!(servcon = malloc (sizeof (ServCon))))
		return NULL;

	servcon->server = server;
	servcon->tcpcon = c;
	servcon->remote_ip = net_peer (servcon->tcpcon->fd);
	servcon->buf = NULL;
	servcon->input = INVALID_INPUT;
	servcon->link = server->list = list_prepend (server->list, servcon);

	return servcon;
}

static void servcon_free (ServCon *servcon, as_bool close)
{
	if (close)
		tcp_close_null (&servcon->tcpcon);

	string_free (servcon->buf);
	input_remove (servcon->input);

	assert (servcon->link);
	servcon->server->list = list_remove_link (servcon->server->list, servcon->link);

	free (servcon);
}

/*****************************************************************************/

/* alloc and init server */
ASHttpServer *as_http_server_create (in_port_t port,
                                     ASHttpServerRequestCb request_cb,
                                     ASHttpServerPushCb push_cb,
                                     ASHttpServerBinaryCb binary_cb)
{
	ASHttpServer *server;

	if(! (server = malloc (sizeof (ASHttpServer))))
		return NULL;

	server->port = port;

	if (! (server->tcpcon = tcp_bind (server->port, FALSE)))
	{
		AS_WARN_1 ("binding to port %d failed", server->port);
		free (server);
		return NULL;
	}

	server->request_cb = request_cb;
	server->push_cb = push_cb;
	server->binary_cb = binary_cb;
	server->list = NULL;

#if 0
	server->banlist_filter = config_get_int (AS_PLUGIN->conf,
	                                         "main/banlist_filter=0");
#endif

	/* wait for incomming connection */
	server->input = input_add (server->tcpcon->fd, (void *)server, INPUT_READ,
							   (InputCallback)server_accept, 0);

	return server;
}

static int servcon_free_itr (ServCon *servcon, void *udata)
{
	servcon_free (servcon, TRUE);

	return TRUE; /* remove */
}

/* free server, close listening port */
void as_http_server_free (ASHttpServer *server)
{
	if (!server)
		return;

	input_remove (server->input);
	tcp_close_null (&server->tcpcon);

	list_foreach_remove (server->list,
			     (ListForeachFunc)servcon_free_itr,
			     NULL);
	free (server);
}

/*****************************************************************************/

static void server_accept (int fd, input_id input, ASHttpServer *server)
{
	ServCon *servcon;
	TCPC *c;

	if (net_sock_error (fd))
	{
		/* cannot happen */
		AS_ERR_1 ("net_sock_error for fd listening on port %d",
				   server->tcpcon->port);
		return;
	}

	if (! (c = tcp_accept (server->tcpcon, FALSE)))
	{
		AS_WARN_1 ("accepting socket from port %d failed",
					server->tcpcon->port);
		return;
	}

	if (!(servcon = servcon_new (server, c)))
		return;

#if 0
	/* deny requests from banned ips */
	if (server->banlist_filter &&
        as_ipset_contains (AS_PLUGIN->banlist, servcon->remote_ip))
	{
		AS_DBG_1 ("denied incoming connection from %s based on banlist",
				   net_ip_str (servcon->remote_ip));

		servcon_free (servcon, TRUE);
		return;
	}
	else
#endif
	{
		AS_HEAVY_DBG_1 ("accepted incoming connection from %s",
		                 net_ip_str (servcon->remote_ip));
	}

	/* wait for data */
	servcon->input = input_add (servcon->tcpcon->fd, (void *)servcon, INPUT_READ,
				    (InputCallback)server_peek, HTSV_REQUEST_TIMEOUT);
}

void as_http_server_pushed (ASHttpServer *server, TCPC *c)
{
	ServCon *servcon;

	if (!(servcon = servcon_new (server, c)))
		return;
	
	/* wait for data */
	servcon->input = input_add (servcon->tcpcon->fd, (void *)servcon, INPUT_READ,
				    (InputCallback)server_peek, HTSV_REQUEST_TIMEOUT);
}

static void server_peek (int fd, input_id input, ServCon *servcon)
{
	unsigned char buf[5];
	int len;

	input_remove (input);
	servcon->input = INVALID_INPUT;

	if (net_sock_error (fd))
	{
		/* remote closed, possibly timeout */
		AS_DBG_1 ("connection from %s closed without receiving any data",
				   net_ip_str (servcon->remote_ip));

		servcon_free (servcon, TRUE);
		return;
	}

	/* we use the first 4 bytes to detrmine the type of connection.
	 * for simplicity we assume that those 4 bytes are available at once.
	 */
	if ((len = tcp_peek (servcon->tcpcon, buf, 4)) != 4)
	{
		AS_DBG_1 ("received less than 4 bytes from %s, closing connection",
				   net_ip_str (servcon->remote_ip));

		servcon_free (servcon, TRUE);
		return;
	}

	buf[4] = 0;

	if (!strcmp (buf, "GET "))
	{
		AS_HEAVY_DBG_2 ("connection from %s is a http request [%s]",
		                 net_ip_str (servcon->remote_ip), buf);
		input_add (servcon->tcpcon->fd, (void *)servcon, INPUT_READ,
				   (InputCallback)server_request, HTSV_REQUEST_TIMEOUT);
	}

/* FIXME: Ares push support */
	else if (!strcmp (buf, "PUSH"))
	{
		AS_HEAVY_DBG_2 ("connection from %s is a push reply [%s]",
		                 net_ip_str (servcon->remote_ip), buf);

		input_add (servcon->tcpcon->fd, (void *)servcon, INPUT_READ,
				   (InputCallback)server_push, HTSV_REQUEST_TIMEOUT);
	}

	else if (!strcmp (buf, "CHAT"))
	{
		/* drop all ares chat requests */
		AS_HEAVY_DBG_2 ("connection from %s is a chat request [%s]. Ignoring.",
		                net_ip_str (servcon->remote_ip), buf);

		servcon_free (servcon, TRUE);
		return;
	}
	else 
	{
		AS_DBG_5 ("connection from %s is binary [%02X%02X%02X%02X]",
				   net_ip_str (servcon->remote_ip), buf[0], buf[1],
				   buf[2], buf[3]);
		input_add (servcon->tcpcon->fd, (void *)servcon, INPUT_READ,
				   (InputCallback)server_binary, HTSV_REQUEST_TIMEOUT);
	}
}

/*****************************************************************************/

static void server_request (int fd, input_id input, ServCon *servcon)
{
	unsigned char buf[1024];
	int len;
	ASHttpHeader *request;

	input_remove (input);
	servcon->input = INVALID_INPUT;

	if (net_sock_error (fd))
	{
		/* remote closed, possibly timeout */
		AS_DBG_1 ("net_sock_error for connection from %s",
				   net_ip_str (servcon->remote_ip));

		servcon_free (servcon, TRUE);
		return;
	}

	/* get us a buffer */
	if (!servcon->buf)
		servcon->buf = string_new (NULL, 0, 0, TRUE);

	/* read a chunk */
	if ((len = tcp_recv (servcon->tcpcon, buf, sizeof (buf))) <= 0)
	{
		AS_DBG_1 ("tcp_recv() < 0 for connection from %s",
				   net_ip_str (servcon->remote_ip));

		servcon_free (servcon, TRUE);
		return;
	}

	/* append to connection buffer */
	if (string_appendu (servcon->buf, buf, len) != len)
	{
		AS_ERR ("Insufficient memory");
		servcon_free (servcon, TRUE);

		return;
	}

	len = servcon->buf->len;

	/* check if we got entire header */
	if (strstr (servcon->buf->str, "\r\n\r\n") == NULL)
	{
		if (len > HTSV_MAX_REQUEST_LEN)
		{
			/* invalid data, close connection */
			AS_DBG_2 ("got more than %d bytes from from %s but no sentinel, closing connection",
					   HTSV_MAX_REQUEST_LEN, net_ip_str (servcon->remote_ip));

			servcon_free (servcon, TRUE);
			return;
		}

		/* wait for more data */
		AS_HEAVY_DBG_2 ("got %d bytes from %s, waiting for more",
						 len, net_ip_str (servcon->remote_ip));
		servcon->input = input_add (servcon->tcpcon->fd, (void *)servcon, INPUT_READ,
					    (InputCallback)server_request, HTSV_REQUEST_TIMEOUT);
		return;
	}

	/* parse header */
	if (!(request = as_http_header_parse (servcon->buf->str, &len)))
	{
		AS_DBG_1 ("parsing header failed for connection from %s, closing connection",
				   net_ip_str (servcon->remote_ip));

		servcon_free (servcon, TRUE);
		return;
	}

#ifdef LOG_HTTP_HEADERS
	servcon->buf->str[len] = 0; /* chops off last '\n', we just use it for debugging */
	AS_DBG_2 ("http request received from %s:\r\n%s",
					 net_ip_str(servcon->remote_ip), servcon->buf->str);
#endif

	/* raise callback */
	if (!servcon->server->request_cb ||
		!servcon->server->request_cb (servcon->server, servcon->tcpcon, request))
	{
		AS_DBG_1 ("Connection from %s closed on callback's request",
				   net_ip_str (servcon->remote_ip));
		as_http_header_free (request);

		servcon_free (servcon, TRUE);
		return;
	}

	servcon_free (servcon, FALSE);
}

static void server_push (int fd, input_id input, ServCon *servcon)
{
	unsigned char buf[1024];
	int len;

	input_remove (input);
	servcon->input = INVALID_INPUT;

	if (net_sock_error (fd))
	{
		/* remote closed, possibly timeout */
		AS_DBG_1 ("net_sock_error for connection from %s",
				   net_ip_str (servcon->remote_ip));

		servcon_free (servcon, TRUE);
		return;
	}

	/* get us a buffer */
	if (!servcon->buf)
		servcon->buf = string_new (NULL, 0, 0, TRUE);

	/* read a chunk */
	if ((len = tcp_recv (servcon->tcpcon, buf, sizeof (buf))) <= 0)
	{
		AS_DBG_1 ("tcp_recv() < 0 for connection from %s",
				   net_ip_str (servcon->remote_ip));

		servcon_free (servcon, TRUE);
		return;
	}

	/* append to connection buffer */
	if (string_appendu (servcon->buf, buf, len) != len)
	{
		AS_ERR ("Insufficient memory");
		servcon_free (servcon, TRUE);
		return;
	}

	len = servcon->buf->len;

	/* check if we got entire header */
	if (strstr (servcon->buf->str, "\n") == NULL)
	{
		if (len > HTSV_MAX_REQUEST_LEN)
		{
			/* invalid data, close connection */
			AS_DBG_2 ("got more than %d bytes from from %s but no sentinel, closing connection",
					   HTSV_MAX_REQUEST_LEN, net_ip_str (servcon->remote_ip));

			servcon_free (servcon, TRUE);
			return;
		}

		/* wait for more data */
		AS_HEAVY_DBG_2 ("got %d bytes from %s, waiting for more",
						 len, net_ip_str (servcon->remote_ip));
		servcon->input = input_add (servcon->tcpcon->fd, (void *)servcon, INPUT_READ,
					    (InputCallback)server_request, HTSV_REQUEST_TIMEOUT);
		return;
	}

#ifdef LOG_HTTP_HEADERS
	servcon->buf->str[len] = 0; /* chops off last '\n', we just use it for debugging */
	AS_HEAVY_DBG_2 ("push reply received from %s:\r\n%s",
					 net_ip_str(servcon->remote_ip), servcon->buf->str);
#endif

	/* raise callback */
	if (!servcon->server->push_cb ||
	    !servcon->server->push_cb (servcon->server, servcon->tcpcon, servcon->buf))
	{
		AS_DBG_1 ("Connection from %s closed on callback's request",
			  net_ip_str (servcon->remote_ip));
		servcon_free (servcon, TRUE);
		return;
	}

	servcon_free (servcon, FALSE);
}

static void server_binary (int fd, input_id input, ServCon *servcon)
{
	input_remove (input);
	servcon->input = INVALID_INPUT;

	if (net_sock_error (fd))
	{
		/* remote closed, possibly timeout */
		AS_DBG_1 ("net_sock_error for connection from %s",
				   net_ip_str (servcon->remote_ip));

		servcon_free (servcon, TRUE);
		return;
	}

	/* raise callback */
	if (!servcon->server->binary_cb ||
		!servcon->server->binary_cb (servcon->server, servcon->tcpcon))
	{
		AS_DBG_1 ("Connection from %s closed on callback's request",
				   net_ip_str (servcon->remote_ip));
		servcon_free (servcon, TRUE);
		return;
	}

	servcon_free (servcon, FALSE);
}

/*****************************************************************************/
