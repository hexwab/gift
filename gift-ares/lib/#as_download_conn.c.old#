/*
 * $Id: as_download_conn.c,v 1.20 2005/01/20 16:06:04 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static as_bool downconn_request (ASDownConn *conn);

static int downconn_http_callback (ASHttpClient *client,
                                   ASHttpClientCbCode code);

static void downconn_push_callback (ASPush *push, TCPC *c);

/*****************************************************************************/

static as_bool downconn_set_state (ASDownConn *conn, ASDownConnState state,
                                   as_bool raise_callback)
{
	conn->state = state;

	if (raise_callback && conn->state_cb)
		return conn->state_cb (conn, conn->state);

	return TRUE;
}

static ASDownConn *downconn_new (void)
{
	ASDownConn *conn;

	if (!(conn = malloc (sizeof (ASDownConn))))
		return NULL;

	conn->source      = NULL;
	conn->hash        = NULL;
	conn->chunk_start = 0;
	conn->chunk_size  = 0;
	conn->client      = NULL;
	conn->push        = NULL;

	conn->queue_pos      = 0;
	conn->queue_len      = 0;
	conn->queue_last_try = 0;
	conn->queue_next_try = 0;

	conn->hist_downloaded = 0;
	conn->hist_time       = 0;
	conn->curr_downloaded = 0;
	conn->request_time    = 0;
	conn->data_time       = 0;

	conn->fail_count       = 0;

	conn->state    = DOWNCONN_UNUSED;
	conn->state_cb = NULL;
	conn->data_cb  = NULL;

	conn->udata1 = NULL;
	conn->udata2 = NULL;

	return conn;
}

static void downconn_reset (ASDownConn *conn)
{
	conn->chunk_start = 0;
	conn->chunk_size = 0;
	as_pushman_remove (AS->pushman, conn->push);
	conn->push = NULL;
	as_hash_free (conn->hash);
	conn->hash = NULL;
}

/* Update stats after a request is complete or cancelled */
static void downconn_update_stats (ASDownConn *conn)
{
	if (conn->request_time == 0)
		return;

	conn->hist_downloaded += conn->curr_downloaded;
	conn->hist_time += time (NULL) - conn->request_time;

	AS_HEAVY_DBG_3 ("Updated stats for %s. last speed: %2.2f kb/s, total speed: %2.2f kb/s",
	                net_ip_str (conn->source->host),
	                (float)conn->curr_downloaded / (time (NULL) - conn->request_time) / 1024,
	                (float)conn->hist_downloaded / conn->hist_time / 1024);

	/* Reset values for last request so we do not add it multiple times */
	conn->curr_downloaded = 0;
	conn->request_time = 0;
}

/*****************************************************************************/

/* Create new download connection from source (copies source). */
ASDownConn *as_downconn_create (ASSource *source, ASDownConnStateCb state_cb,
                                ASDownConnDataCb data_cb)
{
	ASDownConn *conn;

	assert (source);

	if (!(conn = downconn_new ()))
		return NULL;

	conn->source   = as_source_copy (source);
	conn->state_cb = state_cb;
	conn->data_cb  = data_cb;

	return conn;
}

/* Free download connection. */
void as_downconn_free (ASDownConn *conn)
{
	if (!conn)
		return;

	as_downconn_cancel (conn);

	assert (conn->hash == NULL);
	assert (conn->push == NULL);

	as_source_free (conn->source);
	as_http_client_free (conn->client);

	free (conn);
}

/*****************************************************************************/

/* Start this download from this connection with specified piece and hash. */
as_bool as_downconn_start (ASDownConn *conn, ASHash *hash, size_t start,
                           size_t size)
{
	if (conn->state == DOWNCONN_CONNECTING ||
	    conn->state == DOWNCONN_TRANSFERRING)
	{
		assert (0); /* remove later */
		return FALSE;
	}

	assert (start >= 0);
	assert (size > 0);
	assert (conn->hash == NULL);
	
	/* assign new chunk and hash */
	conn->chunk_start = start;
	conn->chunk_size  = size;
	conn->hash        = as_hash_copy (hash);
	conn->curr_downloaded = 0;
	conn->request_time = 0;
	conn->data_time = 0;

	if (!conn->client)
	{
		/* create http client for direct connection */
		conn->client = as_http_client_create (net_ip_str (conn->source->host),
		                                      conn->source->port,
		                                      downconn_http_callback);

		if (!conn->client)
		{
			AS_ERR_2 ("Failed to create http client for %s:%d",
			          net_ip_str (conn->source->host), conn->source->port);

			conn->fail_count++;
			downconn_reset (conn);
			downconn_set_state (conn, DOWNCONN_UNUSED, FALSE);

			return FALSE;
		}

		conn->client->udata = conn;
	}

	/* make request */
	if (!downconn_request (conn))
	{
		AS_ERR_2 ("Failed to send http request to %s:%d",
		          net_ip_str (conn->source->host), conn->source->port);

		conn->fail_count++;
		downconn_reset (conn);
		downconn_set_state (conn, DOWNCONN_UNUSED, FALSE);

		return FALSE;
	}

	/* set state to connecting and raise callback */
	if (!downconn_set_state (conn, DOWNCONN_CONNECTING, TRUE))
		return FALSE; /* connection was freed by callback */

	return TRUE;
}

/* Stop current download. Does not raise callback. State is set to
 * DOWNCONN_UNUSED. 
 */
void as_downconn_cancel (ASDownConn *conn)
{
	if (conn->client)
		as_http_client_cancel (conn->client);

	/* Still update stats since this might be a request cancelled because the
	 * chunk was shrunk */
	downconn_update_stats (conn);

	downconn_reset (conn);
	downconn_set_state (conn, DOWNCONN_UNUSED, FALSE);
}

/*****************************************************************************/

/* Returns average speed of this source collected from past requests in
 * bytes/sec.
 */
unsigned int as_downconn_hist_speed (ASDownConn *conn)
{
	if (conn->hist_time == 0)
		return 0;

	return (conn->hist_downloaded / conn->hist_time);
}

/* Returns average speed of this source collected from past requests and the
 * currently running one in bytes/sec.
 */
unsigned int as_downconn_speed (ASDownConn *conn)
{
	unsigned int speed = 0;

	if (conn->hist_time > 0)
		speed += conn->hist_downloaded / conn->hist_time;

	if (conn->request_time > 0)
	{
		time_t dt = time (NULL) - conn->request_time;
		
		if (dt > 0)
			speed += conn->curr_downloaded / dt;
	}

	return speed;
}

/*****************************************************************************/

/* Suspend transfer using input_suspend_all on http client socket. */
as_bool as_downconn_suspend (ASDownConn *conn)
{
	if (!conn->client || !conn->client->tcpcon)
		return FALSE;

	input_suspend_all (conn->client->tcpcon->fd);
	return TRUE;
}

/* Resume transfer using input_resume_all on http client socket. */
as_bool as_downconn_resume (ASDownConn *conn)
{
	if (!conn->client || !conn->client->tcpcon)
		return FALSE;

	input_resume_all (conn->client->tcpcon->fd);
	return TRUE;
}

/*****************************************************************************/

/* Return connection state as human readable static string. */
const char *as_downconn_state_str (ASDownConn *conn)
{
	switch (conn->state)
	{
	case DOWNCONN_UNUSED:       return "Unused";
	case DOWNCONN_CONNECTING:   return "Connecting";
	case DOWNCONN_TRANSFERRING: return "Transferring";
	case DOWNCONN_FAILED:       return "Failed";
	case DOWNCONN_COMPLETE:     return "Complete";
	case DOWNCONN_QUEUED:       return "Queued";
	}

	return "UNKNOWN";
}

/*****************************************************************************/

static as_bool set_header_encoded (ASHttpHeader *header, char *name,
                                   ASPacket *packet)
{
	char *encoded;
	
	if ((encoded = as_base64_encode (packet->data, packet->used)))
	{
		as_http_header_set_field (header, name, encoded);
		free (encoded);
		return TRUE;
	}

	return FALSE;
}

static void set_header_b6mi (ASHttpHeader *request, ASSource *source)
{
	ASPacket *p;

	p = as_packet_create ();

	/* our IP and port */
	as_packet_put_ip (p, AS->netinfo->outside_ip);
	as_packet_put_le16 (p, AS->netinfo->port);

	/* and our supernode's (use the one we got the result from) */
	as_packet_put_ip (p, source->parent_host);
	as_packet_put_le16 (p, source->parent_port);

	as_encrypt_b6mi (p->data, p->used);
	set_header_encoded (request, "X-B6MI", p);
	
	as_packet_free (p);
}

static void set_header_b6st (ASHttpHeader *request)
{
	ASPacket *p = as_packet_create ();

	as_packet_put_8 (p, 0);    /* unknown */
	as_packet_put_le16 (p, 0); /* unknown */
	as_packet_put_le16 (p, 0); /* unknown */
	as_packet_put_8 (p, 1);    /* unknown */
	as_packet_put_8 (p, 0);    /* % complete? */
	as_packet_put_le32 (p, 0); /* zero */
	as_packet_put_le32 (p, 0); /* unknown */
	as_packet_put_le16 (p, 0); /* unknown */
	as_packet_put_8 (p, 0x11); /* hardcoded */
	as_packet_put_le16 (p, 2); /* unknown */
	as_packet_put_8 (p, 0);    /* unknown */
	as_packet_put_8 (p, 0);    /* unknown */
	as_packet_put_8 (p, 0x80); /* unknown */
	
	as_encrypt_b6st (p->data, p->used);
	set_header_encoded (request, "X-B6St", p);
	
	as_packet_free (p);
}

static as_bool downconn_request (ASDownConn *conn)
{
	ASHttpHeader *request;
	char buf[AS_HASH_BASE64_SIZE + 36];
	char *encoded;
	size_t start, end;

	assert (conn->hash);

	/* create uri and request */
	encoded = as_hash_encode (conn->hash);
	snprintf (buf, sizeof (buf), "sha1:%s", encoded);
	free (encoded);

	if (!(request = as_http_header_request (HTHD_VER_11, HTHD_GET, buf)))
		return FALSE;

	/* add range header (http range is inclusive) */
	assert (conn->chunk_size > 0);
	start = conn->chunk_start;
	end   = conn->chunk_start + conn->chunk_size - 1;
	assert (start <= end); /* start == end is valid since it gets one byte */

	snprintf(buf, sizeof (buf), "bytes=%u-%u", start, end);
	as_http_header_set_field (request, "Range", buf);
	AS_HEAVY_DBG_3 ("Requesting range %s from %s:%d", buf,
	                net_ip_str (conn->source->host), conn->source->port);

	/* add special ares headers */
	set_header_b6mi (request, conn->source);
	set_header_b6st (request);

	/* Send off the request. This takes care of freeing it later. */
	if (!as_http_client_request (conn->client, request, TRUE))
	{
		as_http_header_free (request);
		return FALSE;
	}

	/* wait for http callback */
	return TRUE;
}

/*****************************************************************************/

static as_bool handle_reply (ASHttpClient *client)
{
	ASDownConn *conn = client->udata;
	char *p;

	/* reset queue status */
	conn->queue_pos = 0;
	conn->queue_len = 0;
	conn->queue_last_try = 0;
	conn->queue_next_try = 0;

	switch (client->reply->code)
	{
	case 200:
	case 206:
	{
		unsigned int start, stop, size;

		/* Check that range is ok. */
		p = as_http_header_get_field (client->reply, "Content-Range");

		if (p  && sscanf (p, "bytes=%u-%u/%u", &start, &stop, &size) == 3)
		{
			if (start != conn->chunk_start)
			{
				AS_WARN_4 ("Invalid range start from %s:%d. "
				           "Got %d, expected %d. Aborting.",
				           net_ip_str (conn->source->host),
			               conn->source->port,
						   start, conn->chunk_start);
				break;
			}

			if (stop - start + 1 != conn->chunk_size)
			{
				AS_WARN_4 ("Got different range than request from %s:%d."
				           "Requested size: %d, received: %d. Continuing anyway.",
				           net_ip_str (conn->source->host),
			               conn->source->port,
						   conn->chunk_size, stop - start + 1);
			}

			/* Reset fail count */
			conn->fail_count = 0;

			conn->request_time = conn->data_time = time (NULL);

			/* we are in business */
			downconn_set_state (conn, DOWNCONN_TRANSFERRING, TRUE);

			return TRUE; /* go on with request */
		}

		/* TODO: if missing range headers is common allow this case */
		AS_WARN_2 ("No range header in response from %s:%d."
		           "Aborting to prevent corruption.",
		           net_ip_str (conn->source->host), conn->source->port);
		break;
	}

	case 404:
		/* file not found */
		AS_DBG_2 ("Got 404 from %s:%d", net_ip_str (conn->source->host),
		          conn->source->port);
		break;

	case 503:
	{
		unsigned int pos = 0, len = 0, limit, min, max;
		unsigned int retry = 120; /* seconds */

		p = as_http_header_get_field (client->reply, "X-Queued");

		if (p && sscanf (p, "position=%u,length=%u,limit=%u,pollMin=%u,pollMax=%u",
				         &pos, &len, &limit, &min, &max) == 5)
		{
			retry = (min + max) / 2;
		}

		AS_HEAVY_DBG_5 ("Queued on %s:%d: pos: %d, len: %d, retry: %d",
		                net_ip_str (conn->source->host), conn->source->port,
						pos, len, retry);

		conn->queue_pos = pos;
		conn->queue_len = len;
		conn->queue_last_try = time (NULL);
		conn->queue_next_try = conn->queue_last_try + retry * ESECONDS;

		/* Reset fail count */
		conn->fail_count = 0;

		/* Reset connection since the next request might be for a different
		 * chunk.
		 */
		downconn_reset (conn);
		downconn_set_state (conn, DOWNCONN_QUEUED, TRUE);

		return TRUE; /* this will keep the connection open, with any luck */
	}

	default:
		AS_WARN_4 ("Unknown http response \"%s\" (%d) from %s:%d",
		           client->reply->code_str, client->reply->code,
		           net_ip_str (conn->source->host), conn->source->port);
		break;
	}

	/* Do not continue with request */
	conn->fail_count++;
	downconn_reset (conn);

	/* manually reset http client since the callback may reuse us. */
	as_http_client_cancel (conn->client);

	downconn_set_state (conn, DOWNCONN_FAILED, TRUE);

	return FALSE; /* abort request and close connection */
}

static int downconn_http_callback (ASHttpClient *client,
                                   ASHttpClientCbCode code)
{
	ASDownConn *conn = client->udata;

	switch (code)
	{
	case HTCL_CB_REQUESTING:
		return TRUE;

	case HTCL_CB_CONNECT_FAILED:
		/* This will be called if the connection was _not_ pushed and connect
		 * failed. Send a push request in this case. */
		AS_HEAVY_DBG_2 ("Connect to %s:%d failed. Trying push.",
		                net_ip_str (conn->source->host), conn->source->port);

		/* Cancel and free failed connection */
		as_http_client_cancel (conn->client);
		as_http_client_free (conn->client);
		conn->client = NULL;

		/* Start push request. */
		assert (conn->push == NULL);
		if (!(conn->push = as_pushman_send (AS->pushman,
		                                    downconn_push_callback,
		                                    conn->source, conn->hash)))
		{
			conn->fail_count++;
			downconn_reset (conn);
			/* this may free us */
			downconn_set_state (conn, DOWNCONN_FAILED, TRUE);
			return FALSE;
		}

		conn->push->udata = conn;

		/* Now wait for push reply or timeout */
		return FALSE;

	case HTCL_CB_REQUEST_FAILED:
		conn->fail_count++;
		downconn_reset (conn);
		/* this may free us */
		downconn_set_state (conn, DOWNCONN_FAILED, TRUE);

		return FALSE;

	case HTCL_CB_REPLIED:
		return handle_reply (client);

	case HTCL_CB_DATA:
		/* This may be called in the case of an non-empty queued reply,
		 * ignore it.
		 */
		if (conn->state == DOWNCONN_QUEUED)
			return TRUE;

		assert (conn->state == DOWNCONN_TRANSFERRING);

		conn->data_time = time (NULL);
		conn->curr_downloaded += client->data_len;

		if (conn->data_cb)
			conn->data_cb (conn, client->data, client->data_len);

		return TRUE;

	case HTCL_CB_DATA_LAST:
		/* This is also called for the empty queued reply, ignore it */
		if (conn->state == DOWNCONN_QUEUED)
			return TRUE;

		assert (conn->state == DOWNCONN_TRANSFERRING);
		
		AS_HEAVY_DBG_4 ("HTCL_CB_DATA_LAST (%d/%d) from %s:%d",
		                conn->client->content_received,
		                conn->client->content_length,
		                net_ip_str (conn->source->host), conn->source->port);

		conn->data_time = time (NULL);
		conn->curr_downloaded += client->data_len;

		if (client->data_len > 0 && conn->data_cb)
		{
			if (!conn->data_cb (conn, client->data, client->data_len))
				return FALSE; /* connection was freed / reused  by callback */
		}

		/* update stats */
		downconn_update_stats (conn);

		downconn_reset (conn);
		/* this may free or reuse us */
		downconn_set_state (conn, DOWNCONN_COMPLETE, TRUE);

		return TRUE; /* return value is ignored by http client */
	}

	return TRUE;
}

/*****************************************************************************/

static void downconn_push_callback (ASPush *push, TCPC *c)
{
	ASDownConn *conn = (ASDownConn *) push->udata;

	assert (conn->push == push);
	assert (as_source_equal (push->source, conn->source));

	if (!c || push->state != PUSH_OK)
	{
		/* Fail the download. */
		tcp_close (c);

		conn->fail_count++;
		downconn_reset (conn); /* frees and removes push */
		/* this may free us */
		downconn_set_state (conn, DOWNCONN_FAILED, TRUE);
		return;
	}

	/* Create new http client with pushed connection */
	assert (conn->client == NULL);
	conn->client = as_http_client_create_tcpc (c, downconn_http_callback);

	if (!conn->client)
	{
		AS_ERR_3 ("Failed to create http client for push %d from %s:%d",
		          push->id, net_ip_str (conn->source->host),
		          conn->source->port);

		/* Fail the download. */
		tcp_close (c);

		conn->fail_count++;
		downconn_reset (conn); /* frees and removes push */
		/* this may free us */
		downconn_set_state (conn, DOWNCONN_FAILED, TRUE);
		return;
	}

	conn->client->udata = conn;

	AS_HEAVY_DBG_3 ("Created http client from push %u by %s:%d", push->id,
	                net_ip_str (conn->source->host), conn->source->port);

	/* Send http request. */
	if (!downconn_request (conn))
	{
		AS_ERR_3 ("Failed to send http request to push %d connection from %s:%d",
		          push->id, net_ip_str (conn->source->host),
		          conn->source->port);

		conn->fail_count++;
		downconn_reset (conn); /* frees and removes push */
		downconn_set_state (conn, DOWNCONN_FAILED, TRUE);
		return;
	}

	/* We no longer need the push */
	as_pushman_remove (AS->pushman, push);
	conn->push = NULL;
}

/*****************************************************************************/
