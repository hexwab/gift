/*
 * $Id: as_push.c,v 1.3 2004/09/26 19:49:37 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static void push_connected (int fd, input_id input, ASPush *push);
static as_bool push_timeout (ASPush *push);

/*****************************************************************************/

/* Create push. */
ASPush *as_push_create (as_uint32 id, ASSource *source, ASHash *hash,
                        ASPushCb callback)
{
	ASPush *push;

	if (!(push = malloc (sizeof (ASPush))))
		return FALSE;

	push->source = as_source_copy (source);
	push->hash   = as_hash_copy (hash);
	push->id     = id;

	push->sconn  = NULL;
	push->timer  = INVALID_TIMER;

	push->state    = PUSH_NEW;
	push->callback = callback;

	push->udata = NULL;

	return push;
}

/* Close supernode connection and free push. Does not remove push from its 
 * manager. */
void as_push_free (ASPush *push)
{
	if (!push)
		return;

	/* Close connection, if any */
	if (push->sconn)
		tcp_close (push->sconn);

	/* Remove timer, if any */
	if (push->timer != INVALID_TIMER)
		timer_remove (push->timer);

	as_source_free (push->source);
	as_hash_free (push->hash);

	free (push);
}

/*****************************************************************************/

/* Send push. */
as_bool as_push_send (ASPush *push)
{
	if (push->state != PUSH_NEW)
	{
		assert (0);
		return FALSE;
	}

	if (AS->netinfo->port == 0)
		return FALSE;

	assert (push->sconn == NULL);

	/* TODO: Check if this is one of our supernodes and send push directly
	 * instead of opening new connection. */

	if (!(push->sconn = tcp_open (push->source->shost, push->source->sport,
	                              FALSE)))
	{
		AS_ERR_3 ("Unable to open tcp connection for push %d to %s:%d",
		          push->id, net_ip_str (push->source->shost),
		          push->source->sport);
		return FALSE;
	}

	push->state = PUSH_CONNECTING;

	AS_HEAVY_DBG_3 ("Push %d connecting to %s:%d", push->id,
	                net_ip_str (push->sconn->host), push->sconn->port);

	input_add (push->sconn->fd, (void *)push, INPUT_WRITE,
	           (InputCallback)push_connected, AS_PUSH_CONNECT_TIMEOUT);

	return TRUE;
}

/* Handle pushed connection, i.e. raise callback. */
as_bool as_push_accept (ASPush *push, ASHash *hash, TCPC *c)
{
	/* Make sure hash matches. */
	if (!as_hash_equal (hash, push->hash))
	{
		AS_ERR_3 ("Push %d from %s:%d has wrong hash.", push->id, 
		          net_ip_str (c->host), c->port);

		push->state = PUSH_FAILED;
		push->callback (push, NULL); /* frees us */
		return FALSE;
	}

	/* Make sure this is from the right source. */
	if (c->host != push->source->host)
	{
		/* Should we treat this as error? */

		AS_ERR_4 ("Push %d from %s:%d is not from the host we requested (%s).",
		          push->id, net_ip_str (c->host), c->port,
		          as_source_str (push->source));
	
		push->state = PUSH_FAILED;
		push->callback (push, NULL); /* frees us */
		return FALSE;
	}

	AS_HEAVY_DBG_3 ("Accepted push %d from %s:%d", push->id,
	                net_ip_str (c->host), c->port);

	push->state = PUSH_OK;
	push->callback (push, c); /* frees us */

	return TRUE;
}

/*****************************************************************************/

static ASPacket *push_create_packet (ASPush *push)
{
	ASPacket *p;
	char hex[9];

	/* May potentially have changed since push creation */
	if (AS->netinfo->port == 0)
		return NULL;
	
	if (!(p = as_packet_create ()))
		return NULL;

	as_packet_put_ip (p, push->source->host);
	as_packet_put_le16 (p, AS->netinfo->port);
	as_packet_put_hash (p, push->hash);

	/* Write push id as hex string. */
	snprintf (hex, sizeof (hex), "%08X", push->id);
	as_packet_put_ustr (p, hex, 8);

	as_packet_put_8 (p, 0x61);

	/* Encrypt */
	as_encrypt_push (p->data, p->used, push->source->shost,
	                 push->source->sport);

	/* Add header */
	as_packet_header (p, PACKET_PUSH2);

	return p;
}

static void push_failed (ASPush *push)
{
	tcp_close_null (&push->sconn);
	push->state = PUSH_FAILED;
	push->callback (push, NULL); /* frees us */
}

/*****************************************************************************/

static void push_connected (int fd, input_id input, ASPush *push)
{
	ASPacket *p;

	if (net_sock_error (fd))
	{
		AS_HEAVY_DBG_3 ("Push %d request connect failed to %s:%d", push->id,
		                net_ip_str (push->sconn->host), push->sconn->port);

		push_failed (push);
		return;
	}

	if (!(p = push_create_packet (push)))
	{
		push_failed (push);
		return;
	}

	if (!as_packet_send (p, push->sconn))
	{
		AS_ERR_3 ("Push %d request send to %s:%d failed.", push->id,
		          net_ip_str (push->sconn->host), push->sconn->port);
		as_packet_free (p);
		push_failed (push);
		return;
	}

	as_packet_free (p);

	/* Don't need this connection anymore */
	tcp_close_null (&push->sconn);

	push->state = PUSH_SENT;

	AS_HEAVY_DBG_3 ("Push %d request for %s:%d sent", push->id,
	                net_ip_str (push->source->host), push->source->port);

	/* Wait for pushed connection */
	assert (push->timer == INVALID_TIMER);

	push->timer = timer_add (AS_PUSH_CONNECT_TIMEOUT,
	                         (TimerCallback)push_timeout, push);
}

static as_bool push_timeout (ASPush *push)
{
	timer_remove_zero (&push->timer);

	AS_HEAVY_DBG_3 ("Push %d for %s:%d timed out.", push->id,
	                net_ip_str (push->source->host), push->source->port);

	/* Raise callback */
	push->state = PUSH_FAILED;
	push->callback (push, NULL); /* frees us */

	return FALSE;
}

/*****************************************************************************/
