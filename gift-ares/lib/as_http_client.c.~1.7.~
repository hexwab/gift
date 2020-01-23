/*
 * $Id: as_http_client.c,v 1.7 2004/09/17 21:58:36 HEx Exp $
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

static void client_connected (int fd, input_id input, ASHttpClient *client);
static void client_read_header (int fd, input_id input, ASHttpClient *client);
static void client_read_body (int fd, input_id input, ASHttpClient *client);
static int client_write_data (ASHttpClient *client);

/*****************************************************************************/

static ASHttpClient *client_alloc ()
{
	ASHttpClient *client;

	if(! (client = malloc (sizeof (ASHttpClient))))
		return NULL;

	client->state = HTCL_DISCONNECTED;
	
	client->host = NULL;
	client->ip = 0;
	client->port = 0;
	client->tcpcon = NULL;
	
	client->request = NULL;
	client->reply = NULL;

	client->content_length = 0;
	client->content_received = 0;
	client->data = NULL;
	client->data_len = 0;

	client->callback = NULL;
	client->callback_state = CB_NONE;
	client->udata = NULL;

	return client;
}

static void client_reset (ASHttpClient *client, int close_tcpcon)
{
	if (!client)
		return;

	if (close_tcpcon)
	{
		tcp_close_null (&client->tcpcon);
		client->state = HTCL_DISCONNECTED;
	}
	else 
	{
		client->state = HTCL_CONNECTED;
	}

	as_http_header_free_null (&client->request);
	as_http_header_free_null (&client->reply);

	client->content_length = 0;
	client->content_received = 0;
	client->data_len = 0;
	free (client->data);
	client->data = NULL;

	/* Beautiful this is not.
	 * The problem conceptual.
	 * The solution hidden in darkness.
	 */
	if (client->callback_state == CB_ACTIVE)
		client->callback_state = CB_RESET;
}

/*****************************************************************************/

/* alloc and init client */
ASHttpClient *as_http_client_create (char *host, in_port_t port,
                                     ASHttpClientCallback callback)
{
	ASHttpClient *client;

	assert (host);
	assert (port);
	assert (callback);

	if(! (client = client_alloc ()))
		return NULL;

	client->host = strdup (host);
	client->port = port;
	
	client->callback = callback;

	return client;
}

/* alloc and init client, reuse tcpcon */
ASHttpClient *as_http_client_create_tcpc (TCPC *tcpcon,
                                          ASHttpClientCallback callback)
{
	ASHttpClient *client;

	assert (tcpcon);
	assert (callback);

	if(! (client = client_alloc ()))
		return NULL;

	tcpcon->udata = client;	/* refer back to us */
	client->tcpcon = tcpcon;
	client->ip = tcpcon->host;
	client->host = strdup (net_ip_str (tcpcon->host));
	client->port = tcpcon->port;
	
	client->callback = callback;

	return client;
}

/* free client, closes connection */
void as_http_client_free (ASHttpClient *client)
{
	if (!client)
		return;

	if (client->callback_state == CB_ACTIVE ||
	    client->callback_state == CB_RESET)
	{
		/* free us after callback returns */
		client->callback_state = CB_FREED;
		return;
	}

	/* catch accidential double frees */
	assert (client->callback_state == CB_NONE);

	client_reset (client, TRUE);
	free (client->host);

	free (client);
}

/*****************************************************************************/

/* request file, takes ownership of request,
 * reuses previous connection if possible,
 * tries to keep connection alive if persistent is TRUE
 */
int as_http_client_request (ASHttpClient *client, ASHttpHeader *request,
                            int persistent)
{
	assert (client);
	assert (request);
	assert (client->state == HTCL_DISCONNECTED ||
			client->state == HTCL_CONNECTED);

	client_reset (client, FALSE);
	client->request = request;
	client->persistent = persistent;

	if(! (client->data = malloc (HTCL_DATA_BUFFER_SIZE)))
	{
		client_reset (client, FALSE);
		return FALSE;
	}

	if (client->tcpcon)
	{
		/* reuse connection */
		assert (client->state == HTCL_CONNECTED);

		AS_HEAVY_DBG_3 ("reusing connection to %s [%s]:%d",
		                 client->host, net_ip_str(client->ip),
		                 client->port);

		client_connected (client->tcpcon->fd, 0, client);
		return TRUE;
	}

	/* resolve host */
	if ((client->ip = net_ip (client->host)) == INADDR_NONE)
	{
		struct hostent *he;

		/* TODO: make this non-blocking */
		if (! (he = gethostbyname (client->host)))
		{
			AS_WARN_1 ("gethostbyname failed for host %s", client->host);
			client_reset (client, FALSE);
			return FALSE;
		}

		/* hmm */
		client->ip = *((in_addr_t*)he->h_addr_list[0]);
	}

	/* connect */
	AS_HEAVY_DBG_3 ("opening new tcp connection to %s [%s]:%d",
					 client->host, net_ip_str(client->ip), client->port);

	if (! (client->tcpcon = tcp_open (client->ip, client->port, FALSE)))
	{
		AS_ERR_3 ("ERROR: tcp_open() failed for %s [%s]:%d",
				   client->host, net_ip_str(client->ip), client->port);
		client_reset (client, FALSE);
		return FALSE;
	}

	client->tcpcon->udata = (void *)client;
	client->state = HTCL_CONNECTING;

	/* wait for connection */
	input_add (client->tcpcon->fd, (void *)client, INPUT_WRITE,
			   (InputCallback)client_connected, HTCL_CONNECT_TIMEOUT);

	return TRUE;
}

/* cancel current request by closing connection */
int as_http_client_cancel (ASHttpClient *client)
{
	client_reset (client, TRUE);
	return TRUE;
}

/*****************************************************************************/

static void client_connected (int fd, input_id input, ASHttpClient *client)
{
	String *request_str;
	int len;

	/* in case of a reused connection this is called with input == 0 from
	 * as_http_client_request. input_remove (0) is a nop
	 */
	input_remove (input);

	if (net_sock_error (fd))
	{
		/* connect failed */
		AS_HEAVY_DBG_3 ("net_sock_error(fd) for %s [%s]:%d",
						 client->host, net_ip_str(client->ip), client->port);
		client_reset (client, TRUE);
		client->callback (client, HTCL_CB_CONNECT_FAILED);
		return;
	}

	/* connection established */
	client->state = HTCL_REQUESTING;

	/* notify parent */
	if (! client->callback (client, HTCL_CB_REQUESTING))
	{
		/* parent requested cancellation */
		client_reset (client, TRUE);
		return;
	}

	/* add Host and Connection fields to header */
	as_http_header_set_field (client->request, "Host",
							   stringf ("%s:%d", client->host, client->port));

	as_http_header_set_field (client->request, "Connection",
							   client->persistent ? "Keep-Alive" : "Close");

	/* send the request */
	request_str = as_http_header_compile (client->request);

#ifdef LOG_HTTP_HEADERS
	AS_HEAVY_DBG_3 ("http request sent to %s:%d:\r\n%s", net_ip_str(client->ip),
					 client->port, request_str->str);
#endif

	/* FIXME: short send */
	len = strlen (request_str->str);
	if (tcp_send (client->tcpcon, request_str->str, len) != len)
	{
		AS_WARN_3 ("ERROR: tcp_send failed for %s [%s]:%d",
		           client->host, net_ip_str(client->ip), client->port);
		client_reset (client, TRUE);
		client->callback (client, HTCL_CB_REQUEST_FAILED);
		string_free (request_str);
		return;
	}

	string_free (request_str);

	if (!client->data)
		client->data = malloc (HTCL_DATA_BUFFER_SIZE);
	client->content_length = 0;

	/* wait for reply */
	input_add (client->tcpcon->fd, (void*)client, INPUT_READ,
			   (InputCallback)client_read_header, HTCL_REQUEST_TIMEOUT);
}

static void client_read_header (int fd, input_id input, ASHttpClient *client)
{
	int len, cb_ret;

	input_remove (input);

	if (net_sock_error (fd))
	{
		/* request failed */
		AS_HEAVY_DBG_3 ("net_sock_error(fd) for %s [%s]:%d",
						 client->host, net_ip_str(client->ip), client->port);
		client_reset (client, TRUE);
		client->callback (client, HTCL_CB_REQUEST_FAILED);
		return;
	}

	/* read header */
	len = tcp_recv(client->tcpcon, client->data + client->data_len,
				   HTCL_DATA_BUFFER_SIZE - client->data_len);

	if (len <= 0)
	{
		AS_HEAVY_DBG_3 ("read error while getting header from %s [%s]:%d",
						 client->host, net_ip_str(client->ip), client->port);
		client_reset (client, TRUE);
		client->callback (client, HTCL_CB_REQUEST_FAILED);
		return;
	}

	client->data_len += len;
	len = client->data_len;

	if (! (client->reply = as_http_header_parse (client->data, &len)))
	{
		/* header incomplete or invalid */
		if (client->data_len == HTCL_DATA_BUFFER_SIZE)
		{
			AS_WARN_4 ("Didn't get whole header but read %d bytes from"
						"%s [%s]:%d, closing connection",
						HTCL_DATA_BUFFER_SIZE, client->host,
						net_ip_str(client->ip), client->port);
			client_reset (client, TRUE);
			client->callback (client, HTCL_CB_REQUEST_FAILED);
			return;
		}

		/* wait for more data */
		AS_HEAVY_DBG_3 ("Didn't get whole header from %s [%s]:%d, waiting for more",
						 client->host, net_ip_str(client->ip), client->port);

		input_add (client->tcpcon->fd, (void*)client, INPUT_READ,
				   (InputCallback)client_read_header, HTCL_REQUEST_TIMEOUT);
		return;
	}

	/* we got the reply */
	AS_HEAVY_DBG_5 ("%s [%s]:%d replied with %d (\"%s\")", client->host,
					 net_ip_str(client->ip), client->port,
					 client->reply->code, client->reply->code_str);

#ifdef LOG_HTTP_HEADERS
	{
		char *reply_str = gift_strndup (client->data, len);
		AS_HEAVY_DBG_3 ("http reply from %s:%d:\r\n%s",
						 net_ip_str(client->ip), client->port, reply_str);
		free (reply_str);
	}
#endif

	/* remove header from data while keeping the beginning of the body */
	memmove (client->data, client->data + len, client->data_len - len);
	client->data_len -= len;
	client->content_length = gift_strtol (
	                             as_http_header_get_field (client->reply,
	                             "Content-Length"));
	client->state = HTCL_RECEIVING;

	/* notify parent */
	client->callback_state = CB_ACTIVE;
	cb_ret = client->callback (client, HTCL_CB_REPLIED);

	if (client->callback_state == CB_FREED)
	{
		/* callback tried to free us, do that now */
		client->callback_state = CB_NONE;
		as_http_client_free (client);
		return;
	}

	if (client->callback_state == CB_RESET)
	{
		/* Do nothing else. There may be a new request. */
		client->callback_state = CB_NONE;
		return;
	}

	client->callback_state = CB_NONE;

	if (!cb_ret)
	{
		/* parent requested cancellation */
		client_reset (client, TRUE);
		return;
	}

	if (client->data_len > 0 || client->content_length == 0)
	{
		if (! client_write_data (client))
		{
			/* data end or download aborted */
			return;
		}
	}

	/* read body data */
	input_add (client->tcpcon->fd, (void*)client, INPUT_READ,
		   (InputCallback)client_read_body, HTCL_DATA_TIMEOUT);
}

static void client_read_body (int fd, input_id input, ASHttpClient *client)
{
	int len;
	
	if (net_sock_error (fd))
	{
		/* request failed */
		AS_HEAVY_DBG_3 ("net_sock_error(fd) for %s [%s]:%d",
						 client->host, net_ip_str(client->ip), client->port);
		input_remove (input);
		client_reset (client, TRUE);
		client->callback (client, HTCL_CB_DATA_LAST);
		return;
	}

	/* read next part of body */
	len = tcp_recv(client->tcpcon, client->data, HTCL_DATA_BUFFER_SIZE);

	if (len <= 0)
	{
		/* connection closed */
		AS_HEAVY_DBG_3 ("tcp_recv() <= 0 for %s [%s]:%d",
						 client->host, net_ip_str(client->ip), client->port);
		input_remove (input);
		client_reset (client, TRUE);
		client->callback (client, HTCL_CB_DATA_LAST);
		return;
	}

	client->data_len = len;

	if (! client_write_data (client))
	{
		input_remove (input);
		return;
	}

	/* wait for next chunk */
}

/*****************************************************************************/

/* sends data to parent, returns FALSE if end of data or download cancelled */
static int client_write_data (ASHttpClient *client)
{
	client->content_received += client->data_len;

/*
	AS_HEAVY_DBG_5 ("received %d bytes, %d/%d, from %s:%d", client->data_len,
				   client->content_received, client->content_length,
				   net_ip_str(client->ip), client->port);
*/

#if 0
	assert (client->data_len > 0);
#endif

	if (client->content_received == client->content_length)
	{
		/* a new request may be made from the callback and we need to make
		 * sure it's only reusing the connection if that actually makes
		 * sense.
		 * NOTE: we can't do a client_reset() here because we still need
		 * client->data, etc. in the callback. we can't do it after the
		 * callback either because we would potentially free a new request.
		 * giFT's tendency to call us back from within calls we make to it
		 * highly sucks.
		 * for now we leave the cleaning up to the next request/free for
		 * this client.
		 */
		if (client->persistent)
		{
			AS_HEAVY_DBG_3 ("received all data keeping alive %s [%s]:%d",
							 client->host, net_ip_str(client->ip), client->port);
			client->state = HTCL_CONNECTED;
		}
		else
		{
			AS_HEAVY_DBG_3 ("received all data closing connection to %s [%s]:%d",
							 client->host, net_ip_str(client->ip), client->port);
			tcp_close_null (&client->tcpcon);
			client->state = HTCL_DISCONNECTED;
		}

		/* notify parent */
		client->callback (client, HTCL_CB_DATA_LAST);

		return FALSE; /* remove input */
	}
	else
	{
		int cb_ret;

		/* notify parent */
		client->callback_state = CB_ACTIVE;
		cb_ret = client->callback (client, HTCL_CB_DATA);

		if (client->callback_state == CB_FREED)
		{
			/* callback tried to free us, do that now */
			client->callback_state = CB_NONE;
			as_http_client_free (client);
			return FALSE; /* remove input */
		}

		if (client->callback_state == CB_RESET)
		{
			/* Do nothing else. There may be a new request. */
			client->callback_state = CB_NONE;
			return FALSE;
		}

		client->callback_state = CB_NONE;
	
		if (!cb_ret)
		{
			/* parent requested cancellation */
			client_reset (client, TRUE);
			return FALSE; /* remove input */
		}
	}

	return TRUE; /* continue */
}

/*****************************************************************************/
