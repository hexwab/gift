/*
 * $Id: as_download_conn.c,v 1.27 2006/02/04 00:33:48 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* some time outs in ms */
#define DOWNCONN_CONNECT_TIMEOUT (30*SECONDS)
#define DOWNCONN_REQUEST_TIMEOUT (30*SECONDS)
#define DOWNCONN_DATA_TIMEOUT    (30*SECONDS)

#define DOWNCONN_MAX_HEADER_SIZE 4096

/*
#define LOG_HTTP_REPLY
*/

/*****************************************************************************/

/* Connect to source if necessary and then send request. */
static as_bool downconn_request (ASDownConn *conn);

static void downconn_connected (int fd, input_id input, ASDownConn *conn);
static as_bool downconn_send_request (ASDownConn *conn);
static as_bool downconn_request_timeout (ASDownConn *conn);
static void downconn_read_header (int fd, input_id input, ASDownConn *conn);
static void downconn_handle_reply (ASDownConn *conn, ASHttpHeader *reply);
static as_bool downconn_data_timeout (ASDownConn *conn);
static void downconn_read_body (int fd, input_id input, ASDownConn *conn);

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

	conn->source       = NULL;
	conn->hash         = NULL;
	conn->chunk_start  = 0;
	conn->chunk_size   = 0;
	conn->tcpcon       = NULL;
	conn->tcpcon_timer = INVALID_TIMER;
	conn->recv_buf     = NULL;
	conn->reply_key    = 0x00; /* anything will do since it's stupid anyway */
	conn->keep_alive   = FALSE;
	conn->push         = NULL;

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
	timer_remove_zero (&conn->tcpcon_timer);
	as_packet_free (conn->recv_buf);
	conn->recv_buf = NULL;
	conn->reply_key = 0x00; /* anything will do since it's stupid anyway */
	conn->keep_alive = FALSE;
	as_pushman_remove (AS->pushman, conn->push);
	conn->push = NULL;
	as_hash_free (conn->hash);
	conn->hash = NULL;
}

/* Update stats after a request is complete or cancelled */
static void downconn_update_stats (ASDownConn *conn)
{
	time_t delta;

	if (conn->request_time == 0)
		return;

	delta = time (NULL) - conn->request_time;

	if (delta == 0)
		delta = 1;

	conn->hist_downloaded += conn->curr_downloaded;
	conn->hist_time += delta;

	AS_HEAVY_DBG_3 ("Updated stats for %s. last speed: %2.2f kb/s, total speed: %2.2f kb/s",
	                net_ip_str (conn->source->host),
	                (float)conn->curr_downloaded / delta / 1024,
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
	tcp_close_null (&conn->tcpcon);

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
	if (conn->tcpcon)
		tcp_close_null (&conn->tcpcon);

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
	if (!conn->tcpcon)
		return FALSE;

	input_suspend_all (conn->tcpcon->fd);
	return TRUE;
}

/* Resume transfer using input_resume_all on http client socket. */
as_bool as_downconn_resume (ASDownConn *conn)
{
	if (!conn->tcpcon)
		return FALSE;

	input_resume_all (conn->tcpcon->fd);
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

/* Connect to source if necessary and then send request. */
static as_bool downconn_request (ASDownConn *conn)
{
	assert (conn->recv_buf == NULL);
	assert (conn->tcpcon_timer == INVALID_TIMER);

	if (!(conn->recv_buf = as_packet_create ()))
	{
		AS_ERR ("Insufficient memory");
		return FALSE;
	}

	if (conn->tcpcon)
	{
		/* reuse connection */
		AS_HEAVY_DBG_2 ("Reusing connection to %s:%d",
		                net_ip_str(conn->tcpcon->host), conn->tcpcon->port);

		if (!downconn_send_request (conn))
		{
			tcp_close_null (&conn->tcpcon);
			return FALSE;
		}

		return TRUE;
	}

	/* connect */
	AS_HEAVY_DBG_2 ("Opening new tcp connection to %s:%d",
					net_ip_str(conn->source->host), conn->source->port);

	if (!(conn->tcpcon = tcp_open (conn->source->host, conn->source->port,
	                               FALSE)))
	{
		AS_ERR_2 ("tcp_open() failed for %s:%d",
				  net_ip_str(conn->source->host), conn->source->port);
		return FALSE;
	}

	/* wait for connection */
	input_add (conn->tcpcon->fd, (void *)conn, INPUT_WRITE,
			   (InputCallback)downconn_connected, DOWNCONN_CONNECT_TIMEOUT);

	return TRUE;
}

static void downconn_connected (int fd, input_id input, ASDownConn *conn)
{
	input_remove (input);

	if (net_sock_error (fd))
	{
		/* Connect failed */
		AS_HEAVY_DBG_2 ("Connect to %s:%d failed. Trying push.",
		                net_ip_str (conn->source->host), conn->source->port);

		/* Free failed connection */
		tcp_close_null (&conn->tcpcon);

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
			return;
		}

		conn->push->udata = conn;

		/* Now wait for push reply or timeout */
		return;
	}

	/* try to send request */
	if (!downconn_send_request (conn))
	{
		/* request failed */
		conn->fail_count++;
		downconn_reset (conn);
		tcp_close_null (&conn->tcpcon);
		/* this may free us */
		downconn_set_state (conn, DOWNCONN_FAILED, TRUE);
		return;
	}
}

/*****************************************************************************/

static as_bool request_put_b6st (ASPacket *request)
{
	ASPacket *p;

	if (!(p = as_packet_create ()))
		return FALSE;

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

	as_packet_put_le16 (request, (as_uint16) p->used);
	as_packet_put_8 (request, 0x06);
	as_packet_put_ustr (request, p->data, p->used);

	as_packet_free (p);
	return TRUE;
}

static as_bool request_put_0a (ASPacket *request)
{
	ASPacket *p;

	if (!(p = as_packet_create ()))
		return FALSE;

	/* TODO: Look this up in Ares sources and use proper values */

	as_packet_put_8 (p, 0x01); /* unknown */
	as_packet_put_le16 (p, 0); /* unknown */
	as_packet_put_le16 (p, 0xc0); /* unknown */
	as_packet_put_8 (p, 0x0f); /* unknown */
	as_packet_put_8 (p, 0); /* unknown */
	as_packet_put_le32 (p, 0); /* unknown */
	as_packet_put_le32 (p, 0); /* unknown */
	as_packet_put_le16 (p, 0); /* unknown */
	as_packet_put_8 (p, 0x11);
	as_packet_put_le16 (p, 0x04); /* unknown */
	as_packet_put_8 (p, 0); /* unknown */
	as_packet_put_8 (p, 0); /* unknown */
	as_packet_put_8 (p, 0xff); /* unknown */

	if (!as_encrypt_transfer_0a (p))
	{
		as_packet_free (p);
		return FALSE;
	}

	as_packet_put_le16 (request, (as_uint16) p->used);
	as_packet_put_8 (request, 0x0a);
	as_packet_put_ustr (request, p->data, p->used);

	as_packet_free (p);
	return TRUE;
}

static as_bool downconn_send_request (ASDownConn *conn)
{
	ASPacket *req;
	as_uint32 start, end;

	assert (conn->hash);
	assert (conn->chunk_size > 0);
	start = conn->chunk_start;
	end   = conn->chunk_start + conn->chunk_size - 1;
	assert (start <= end); /* start == end is valid since it gets one byte */

	if (!(req = as_packet_create ()))
		return FALSE;

	/* command */
	as_packet_put_8 (req, 0x01); /* GET */

	/* encryption */
	as_packet_put_le16 (req, 3);
	as_packet_put_8 (req, 0x32);
	as_packet_put_8 (req, 1); /* enc type */
	as_packet_put_le16 (req, conn->reply_key); /* reply key */

	/* hash */
	as_packet_put_le16 (req, 20);
	as_packet_put_8 (req, 0x01);
	as_packet_put_hash (req, conn->hash);
	
	/* username */
	as_packet_put_le16 (req, (as_uint16) strlen (AS_CONF_STR (AS_USER_NAME)));
	as_packet_put_8 (req, 0x02);
	as_packet_put_ustr (req, AS_CONF_STR (AS_USER_NAME),
	                    strlen (AS_CONF_STR (AS_USER_NAME)));

	/* add stats data used for queueing by remote node */
	request_put_b6st (req);
	request_put_0a (req);
	
	/* range */
	as_packet_put_le16 (req, 8);
	as_packet_put_8 (req, 0x07);
	as_packet_put_le32 (req, start);
	as_packet_put_le32 (req, end);

	/* extended range */
	as_packet_put_le16 (req, 16);
	as_packet_put_8 (req, 0x0b);
	as_packet_put_le32 (req, start);
	as_packet_put_le32 (req, 0); /* hiword of start */
	as_packet_put_le32 (req, end);
	as_packet_put_le32 (req, 0); /* hiword of end */

	/* client name and version */
	as_packet_put_le16 (req, (as_uint16) strlen (AS_DOWNLOAD_AGENT));
	as_packet_put_8 (req, 0x09);
	as_packet_put_ustr (req, AS_DOWNLOAD_AGENT, strlen (AS_DOWNLOAD_AGENT));

	/* node info */
	as_packet_put_le16 (req, 16);
	as_packet_put_8 (req, 0x03);
	as_packet_put_ip (req, conn->source->parent_host);
	as_packet_put_le16 (req, conn->source->parent_port);
	as_packet_put_ip (req, AS->netinfo->outside_ip);
	as_packet_put_le16 (req, AS->netinfo->port);
	as_packet_put_ip (req, net_local_ip (conn->tcpcon->fd, NULL));

	/* alt sources */
	as_packet_put_le16 (req, 0);
	as_packet_put_8 (req, 0x08);

#if 0
	/* phash */
	as_packet_put_le16 (req, 1);
	as_packet_put_8 (req, 0x0c);
	as_packet_put_8 (req, 1);
#endif

	/* encrypt request */
	if (!as_encrypt_transfer_request (req))
	{
		as_packet_free (req);
		return FALSE;
	}

	AS_HEAVY_DBG_4 ("Requesting range [%u,%u) from %s:%d", start, end,
	                net_ip_str (conn->source->host), conn->source->port);

	/* send using write queue */
	if (!as_packet_send (req, conn->tcpcon))
	{
		AS_WARN_2 ("as_packet_send failed for %s:%d",
		           net_ip_str(conn->tcpcon->host), conn->tcpcon->port);
		as_packet_free (req);
		return FALSE;
	}

	as_packet_free (req);

	/* wait for reply using timeout */
	input_add (conn->tcpcon->fd, (void*)conn, INPUT_READ,
			   (InputCallback)downconn_read_header, 0);

	assert (conn->tcpcon_timer == INVALID_TIMER);
	conn->tcpcon_timer = timer_add (DOWNCONN_REQUEST_TIMEOUT,
	                               (TimerCallback)downconn_request_timeout,
	                               conn);

	return TRUE;
}

static void downconn_request_failed (ASDownConn *conn)
{
	conn->fail_count++;
	downconn_reset (conn);
	tcp_close_null (&conn->tcpcon);

	/* this may free us */
	downconn_set_state (conn, DOWNCONN_FAILED, TRUE);
}

/* we sent a request but didn't get a reply in time */
static as_bool downconn_request_timeout (ASDownConn *conn)
{
	AS_ERR_2 ("Download request timeout for %s:%d",
	          net_ip_str (conn->tcpcon->host), conn->tcpcon->port);

	downconn_request_failed (conn);
	return FALSE;
}

static void downconn_read_header (int fd, input_id input, ASDownConn *conn)
{
	int len, http_header_len;
	as_uint8 buf[1024];
	ASPacket *reply;
	ASHttpHeader *header;
	as_uint16 key;
	char *p;

	input_remove (input);

	if (net_sock_error (fd))
	{
		/* request failed */
		AS_HEAVY_DBG_2 ("net_sock_error(fd) for %s:%d",
						net_ip_str (conn->tcpcon->host), conn->tcpcon->port);

		/* this may free us */
		downconn_request_failed (conn);
		return;
	}

	/* read data */
	if ((len = tcp_recv(conn->tcpcon, buf, sizeof(buf))) <= 0)
	{
		AS_HEAVY_DBG_2 ("read error while getting header from %s:%d",
						net_ip_str (conn->tcpcon->host), conn->tcpcon->port);

		/* this may free us */
		downconn_request_failed (conn);
		return;
	}

	/* append data to recv buffer */
	as_packet_put_ustr (conn->recv_buf, buf, len);

	/* try decrypt header */
	as_packet_rewind (conn->recv_buf);
	if (!(reply = as_packet_create_copy (conn->recv_buf, as_packet_remaining (conn->recv_buf))))
	{
		downconn_request_failed (conn);
		return;
	}

	key = conn->reply_key;
	if (!as_decrypt_transfer_reply (reply, &key))
	{
		/* decrypt failed => wait for more data */
		AS_HEAVY_DBG_2 ("Didn't get entire encryption header from %s:%d, waiting for more",
						net_ip_str(conn->tcpcon->host), conn->tcpcon->port);
		
		as_packet_free (reply);
		input_add (conn->tcpcon->fd, (void*)conn, INPUT_READ,
				   (InputCallback)downconn_read_header, 0);
		return;
	}

	/* try to parse decrypted header */
	http_header_len = as_packet_size (reply);

	if (!(header = as_http_header_parse (reply->data, &http_header_len)))
	{
		/* header incomplete or invalid */
		if (as_packet_size (conn->recv_buf) >= DOWNCONN_MAX_HEADER_SIZE)
		{
			AS_WARN_3 ("Didn't get entire http header but read %d bytes from"
						"%s:%d, closing connection",
						DOWNCONN_MAX_HEADER_SIZE,
						net_ip_str(conn->tcpcon->host), conn->tcpcon->port);

			downconn_request_failed (conn);
			return;
		}

		/* wait for more data */
		AS_HEAVY_DBG_2 ("Didn't get entire http header from %s:%d, waiting for more",
						net_ip_str(conn->tcpcon->host), conn->tcpcon->port);

		as_packet_free (reply);
		input_add (conn->tcpcon->fd, (void*)conn, INPUT_READ,
				   (InputCallback)downconn_read_header, 0);
		return;
	}

	/* we got the reply */
	AS_HEAVY_DBG_4 ("%s:%d replied with %d (\"%s\")",
					net_ip_str(conn->tcpcon->host), conn->tcpcon->port,
					header->code, header->code_str);

#ifdef LOG_HTTP_REPLY
	{
		char *reply_str = gift_strndup (reply->data, http_header_len);
		AS_HEAVY_DBG_3 ("http reply from %s:%d:\r\n%s",
						net_ip_str(conn->tcpcon->host), conn->tcpcon->port,
		                reply_str);
		free (reply_str);
	}
#endif

	/* Check if we can keep connection alive. HTTP 1.1 specifies that all
	 * connections are keep-alive unless 'Connection: Close' is present.
	 * The Ares author is not aware of this thus the assumption here is that
	 * connections are closed unless 'Connection: Keep-Alive' is specified.
	 */
	p = as_http_header_get_field (header, "Connection");
	conn->keep_alive = (gift_strcasecmp (p, "Keep-Alive") == 0);

	/* update key so it is correct for decrypting body later */
	conn->reply_key = key;

	/* remove both encryption and http header from data */
	assert (as_packet_size (reply) >= (unsigned int) http_header_len);
	reply->read_ptr = reply->data + http_header_len;
	as_packet_truncate (reply);

	/* free encrypted packet and replace it with decrypted body we already got */
	as_packet_free (conn->recv_buf);
	conn->recv_buf = reply;

	/* remove reply timeout */
	timer_remove_zero (&conn->tcpcon_timer);

	/* decide what to do based on reply. this may free us */
	downconn_handle_reply (conn, header);
	return;
}

static void downconn_handle_reply (ASDownConn *conn, ASHttpHeader *reply)
{
	char *p;

	/* reset queue status */
	conn->queue_pos = 0;
	conn->queue_len = 0;
	conn->queue_last_try = 0;
	conn->queue_next_try = 0;

	switch (reply->code)
	{
	case 200:
	case 206:
	{
		unsigned int start, stop, size;

		/* Check that range is ok. */
		p = as_http_header_get_field (reply, "Content-Range");

		/* Accept both Ares and HTTP style range header. */
		if (p  && (sscanf (p, "bytes=%u-%u/%u", &start, &stop, &size) == 3 ||
		           sscanf (p, "bytes %u-%u/%u", &start, &stop, &size) == 3))
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
			as_http_header_free (reply);
			conn->fail_count = 0;
			conn->request_time = conn->data_time = time (NULL);

			/* we are in business */
			if (!downconn_set_state (conn, DOWNCONN_TRANSFERRING, TRUE))
				return; /* we were freed by callback */

			/* schedule read of the rest of the data */
			input_add (conn->tcpcon->fd, (void*)conn, INPUT_READ,
					   (InputCallback)downconn_read_body, 0);

			assert (conn->tcpcon_timer == INVALID_TIMER);
			conn->tcpcon_timer = timer_add (DOWNCONN_DATA_TIMEOUT,
			                               (TimerCallback)downconn_data_timeout,
			                               conn);

			/* Write the remaining decrypted data we already got with header.
			 *
             * NOTE: We are doing this after adding the downconn_read_body
             *       input because our gift wrapper will always return FALSE
			 *       from the data_cb. See dl_data_callback in asp_download.c.
             */
			if (as_packet_size (conn->recv_buf) > 0)
			{
				conn->data_time = time (NULL);
				conn->curr_downloaded += as_packet_size (conn->recv_buf);

				if (conn->data_cb)
				{
					if (!conn->data_cb (conn, conn->recv_buf->data,
					                    conn->recv_buf->used))
					{
						return; /* we were freed by callback */
					}
				}
			}

			/* This may not actually be called, see the note above. The packet
			 * either has already been freed or it will be at a later time in
			 * downconn_reset.
			 */
			as_packet_free (conn->recv_buf);
			conn->recv_buf = NULL;

			return; /* wait for file data */
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

		p = as_http_header_get_field (reply, "X-Queued");

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
		as_http_header_free (reply);
		downconn_reset (conn);
		if (!conn->keep_alive)
			tcp_close_null (&conn->tcpcon);
		downconn_set_state (conn, DOWNCONN_QUEUED, TRUE);

		return; /* maybe keep connection open and wait for new request */
	}

	default:
		AS_WARN_4 ("Unknown http response \"%s\" (%d) from %s:%d",
		           reply->code_str, reply->code,
		           net_ip_str (conn->source->host), conn->source->port);
		break;
	}


	/* Do not continue with request */
	as_http_header_free (reply);
	conn->fail_count++;
	downconn_reset (conn);
	tcp_close_null (&conn->tcpcon);

	/* may free or reuse us */
	downconn_set_state (conn, DOWNCONN_FAILED, TRUE);

	return;
}

/* we haven't received data in a while, close connection */
static as_bool downconn_data_timeout (ASDownConn *conn)
{
	AS_ERR_2 ("Download data timeout for %s:%d",
	          net_ip_str (conn->tcpcon->host), conn->tcpcon->port);

	downconn_reset (conn);
	tcp_close_null (&conn->tcpcon);

	/* this may free us */
	downconn_set_state (conn, DOWNCONN_FAILED, TRUE);

	return FALSE; /* remove timer */
}

static void downconn_read_body (int fd, input_id input, ASDownConn *conn)
{
	int len;
	char buf[4096];

	assert (conn->state == DOWNCONN_TRANSFERRING);
	
	if (net_sock_error (fd))
	{
		/* request failed */
		AS_HEAVY_DBG_2 ("net_sock_error(fd) for %s:%d",
						net_ip_str(conn->tcpcon->host), conn->tcpcon->port);
		input_remove (input);
		downconn_reset (conn);
		tcp_close_null (&conn->tcpcon);

		/* this may free us */
		downconn_set_state (conn, DOWNCONN_FAILED, TRUE);
		return;
	}

	/* read next part of body */
	if ((len = tcp_recv (conn->tcpcon, buf, sizeof (buf))) <= 0)
	{
		/* connection closed */
		AS_HEAVY_DBG_2 ("tcp_recv() <= 0 for %s:%d",
						net_ip_str(conn->tcpcon->host), conn->tcpcon->port);
		input_remove (input);
		downconn_reset (conn);
		tcp_close_null (&conn->tcpcon);

		/* this may free us */
		downconn_set_state (conn, DOWNCONN_FAILED, TRUE);
		return;
	}

	conn->data_time = time (NULL);
	conn->curr_downloaded += len;

	/* reset data timeout */
	timer_reset (conn->tcpcon_timer);

	/* decrypt data */
	as_decrypt_transfer_body (buf, len, &conn->reply_key);

	/* write data */
	if (conn->data_cb)
	{
		if (!conn->data_cb (conn, buf, len))
			return; /* connection was freed / reused  by callback */
	}

	if (conn->curr_downloaded == conn->chunk_size)
	{
		input_remove (input);

		/* update stats */
		downconn_update_stats (conn);

		downconn_reset (conn);
		if (!conn->keep_alive)
			tcp_close_null (&conn->tcpcon);

		/* this may free or reuse us */
		downconn_set_state (conn, DOWNCONN_COMPLETE, TRUE);
		return;
	}

	return; /* wait for more data */
}

/*****************************************************************************/

static void downconn_push_callback (ASPush *push, TCPC *c)
{
	ASDownConn *conn = (ASDownConn *) push->udata;

	assert (conn->push == push);
	assert (conn->tcpcon == NULL);
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

	/* assign tcp connection to us */
	conn->tcpcon = c;

	AS_HEAVY_DBG_3 ("Assigned pushed connection %u from %s:%d to downconn",
	                push->id, net_ip_str (conn->source->host),
	                conn->source->port);

	/* Send request. */
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
