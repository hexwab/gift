/*
 * ft_session.c
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

#include "ft_openft.h"

#include "nb.h"
#include "ft_netorg.h"

#include "ft_session.h"

/*****************************************************************************/

/* do not turn this on unless you have surely lost your mind */
/* #define FT_SESSION_DEBUG */

/*****************************************************************************/

static int session_auth_packet (Connection *c, FTPacket *packet)
{
	FTCommand cmd;
	int       ret = FALSE;

	if (!packet)
		return FALSE;

	cmd = (FTCommand)ft_packet_command (packet);

	switch (FT_SESSION(c)->stage)
	{
	 case 3:
		if (cmd >= FT_SESSION_REQUEST && cmd <= FT_SESSION_RESPONSE) ret = TRUE;
	 case 2:
		if (cmd >= FT_NODEINFO_REQUEST && cmd <= FT_SESSION_REQUEST) ret = TRUE;
	 case 1:
		if (cmd >= FT_VERSION_REQUEST && cmd <= FT_VERSION_RESPONSE) ret = TRUE;
		break;
	 case 4:
		ret = TRUE;
		break;
	}

	return ret;
}

static void handle_packet (Connection *c, unsigned char *data, size_t len)
{
	FTPacket *packet;
	int       ret = FALSE;

	packet = ft_packet_unserialize (data, len);

#ifdef FT_SESSION_DEBUG
	TRACE_SOCK (("%s", ft_packet_fmt (packet)));
#endif /* FT_SESSION_DEBUG */

	if (session_auth_packet (c, packet))
		ret = protocol_handle (openft_p, c, packet);

	if (!ret)
	{
		TRACE_SOCK (("%i(0x%08x): failed %s", FT_SESSION(c)->stage,
		             (unsigned int)FT_NODE(c)->version, ft_packet_fmt (packet)));

		ft_session_stop (c);
	}

	ft_packet_free (packet);
}

static void session_handle (Protocol *p, Connection *c)
{
	NBRead   *nb;
	ft_uint16 len;
	int       n;

	/* grab the data buffer */
	nb = nb_active (c->fd);

	/* nb->flag, if non-zero, will indicate the total length of the data
	 * held within the OpenFT packet */
	len = nb->flag + FT_PACKET_HEADER;

	if ((n = nb_read (nb, len, NULL)) <= 0)
	{
		ft_session_stop (c);
		return;
	}

	if (!nb->term)
		return;

	/* OpenFT packet length directly from the header */
	len = net_get16 (nb->data, TRUE);

	if (nb->flag || len == 0)
	{
		nb->flag = 0;
		handle_packet (c, nb->data, (size_t)nb->len);
	}
	else if (!nb->flag)
	{
		/* we read a header block, adjust the flag hack so that we read
		 * the complete packet next time */
		nb->term = FALSE;
		nb->flag = len;
	}
}

/*****************************************************************************/

time_t ft_session_uptime (Connection *c)
{
	time_t start;
	time_t curr;

	if (!FT_SESSION(c))
		return 0;

	start = FT_SESSION(c)->start;
	curr  = time (NULL);

	if (!start)
		return 0;

	return MAX (0, curr - start);
}

/*****************************************************************************/

static int session_start (Connection *c, int incoming)
{
	if (!FT_SESSION(c))
	{
		if (!(FT_SESSION(c) = malloc (sizeof (FTSession))))
			return FALSE;
	}

	memset (FT_SESSION(c), 0, sizeof (FTSession));
	return TRUE;
}

int ft_session_start (Connection *c, int incoming)
{
	if (!session_start (c, incoming))
		return FALSE;

	node_state_set (c, NODE_CONNECTED);

	ft_session_stage (c, 0);

	input_add (openft_p, c, INPUT_READ,
	           (InputCallback) session_handle, TRUE);

	return TRUE;
}

/*****************************************************************************/

static void session_flush_queue (Connection *c, int psend)
{
	FTPacket *packet;

	if (!c || !FT_SESSION(c)->queue)
		return;

	while ((packet = list_queue_shift (FT_SESSION(c)->queue)))
	{
		TRACE_SOCK (("%i: flushing %s", psend, ft_packet_fmt (packet)));

		if (!psend)
		{
			ft_packet_free (packet);
			continue;
		}

		assert (session_auth_packet (c, packet) == TRUE);

		/* this will cleanup the memory eventually */
		ft_packet_send (c, packet);
	}
}

static void session_stop (Connection *c)
{
	time_t uptime;

	if (!FT_SESSION(c))
		return;

	/* increment the total uptime and last session information */
	uptime = ft_session_uptime (c);
	FT_NODE(c)->uptime += uptime;
	FT_NODE(c)->last_session = FT_SESSION(c)->start + uptime;

	/* remove the handshaking timeout */
	timer_remove (FT_SESSION(c)->start_timer);
	FT_SESSION(c)->start_timer = 0;

	/* reset stage */
	FT_SESSION(c)->stage = 0;

	/* flush packet write queue */
	session_flush_queue (c, FALSE);
	list_queue_free (FT_SESSION(c)->queue);
	FT_SESSION(c)->queue = NULL;

	/* indicate that the session has not begun */
	FT_SESSION(c)->start = 0;

	/* free, this may change in the future */
	free (FT_SESSION(c));
	FT_SESSION(c) = NULL;
}

void ft_session_stop (Connection *c)
{
	if (!c)
		return;

	session_stop (c);
	node_disconnect (c);
}

/*****************************************************************************/

static int handshake_timeout (Connection *c)
{
	TRACE_SOCK (("handshaking took too long"));

	/* drop the bastards */
	ft_session_stop (c);

	return FALSE;
}

static void stage_1 (Connection *c)
{
	/* create a timer requiring that the session reach stage 4 before it
	 * is raised... */
	FT_SESSION(c)->start_timer =
		timer_add (2 * MINUTES, (TimerCallback) handshake_timeout, c);

	/* request that the remote node deliver their version to us */
	ft_packet_sendva (c, FT_VERSION_REQUEST, 0, NULL);
}

static void stage_2 (Connection *c)
{
	/* exchange node capabilities */
	ft_packet_sendva (c, FT_NODECAP_REQUEST, 0, NULL);

	/* leech as much info from this node as possible... */
	ft_packet_sendva (c, FT_NODELIST_REQUEST, 0, NULL);

	/* always attempt to update our node information */
	ft_packet_sendva (c, FT_NODEINFO_REQUEST, 0, NULL);
}

static void stage_3 (Connection *c)
{
	/* notify the remote node that we have finished port verification and are
	 * ready to move to stage 4 whenever they are */
	ft_packet_sendva (c, FT_SESSION_REQUEST, 0, NULL);
}

static void stage_4 (Connection *c)
{
	/* handshaking complete, remove timer */
	timer_remove (FT_SESSION(c)->start_timer);
	FT_SESSION(c)->start_timer = 0;

	/* notify this node that we have moved to session 4, it is possible (but
	 * not required) that the remote node is currently in stage 3 and this
	 * message will bring them up to 4 with us so that they do not think the
	 * queue flushes are out of order */
	ft_packet_sendva (c, FT_SESSION_RESPONSE, 0, "h", TRUE);

	/* write all the data we queued up to be released when the handshaking
	 * completed (as it just did) */
	session_flush_queue (c, TRUE);

	/* record this time as the beginning of the current session */
	FT_SESSION(c)->start = time (NULL);
}

void ft_session_stage (Connection *c, unsigned char current)
{
	if (!c)
		return;

	if (FT_SESSION(c)->stage != current)
		return;

	FT_SESSION(c)->stage++;

#ifdef FT_SESSION_DEBUG
	TRACE_SOCK (("stage = %i", (int)FT_SESSION(c)->stage));
#endif /* FT_SESSION_DEBUG */

	switch (FT_SESSION(c)->stage)
	{
	 case 1:  stage_1 (c); break;
	 case 2:  stage_2 (c); break;
	 case 3:  stage_3 (c); break;
	 case 4:  stage_4 (c); break;
	 default: abort ();    break;
	}
}

int ft_session_queue (Connection *c, FTPacket *packet)
{
	if (!c || !FT_SESSION(c) || !packet)
		return FALSE;

	/* this packet is authorized to go out at this time, deliver it now */
	if (session_auth_packet (c, packet))
		return FALSE;

	TRACE_SOCK (("queueing %s", ft_packet_fmt (packet)));

	/* this packet must be queued and delivered when the appropriate stage
	 * has been set (see above) */
	if (!FT_SESSION(c)->queue)
		FT_SESSION(c)->queue = list_queue_new (NULL);

	list_queue_push (FT_SESSION(c)->queue, packet);
	return TRUE;
}

/*****************************************************************************/

static void connect_complete (Connection *c, int incoming)
{
	input_remove (c);

	if (net_sock_error (c->fd))
	{
		ft_session_stop (c);
		return;
	}

	if (!ft_session_start (c, incoming))
		ft_session_stop (c);
}

static void outgoing_complete (Protocol *p, Connection *c)
{
	connect_complete (c, FALSE);
}

int ft_session_connect (Connection *c)
{
	if (c->fd >= 0)
		return -1;

	if (FT_NODE(c)->state != NODE_DISCONNECTED)
		return -1;

	if (!conn_auth (c, TRUE))
		return -1;

	c->fd = net_connect (net_ip_str (FT_NODE(c)->ip), FT_NODE(c)->port, FALSE);
	FT_NODE(c)->fd = c->fd;

	node_state_set (c, NODE_CONNECTING);
	FT_NODE(c)->incoming = FALSE;

	input_add (openft_p, c, INPUT_WRITE,
			   (InputCallback) outgoing_complete, TRUE);

	return c->fd;
}

static void incoming_complete (Protocol *p, Connection *c)
{
	connect_complete (c, TRUE);
}

void ft_session_incoming (Protocol *p, Connection *listen)
{
	Connection *c;
	int         fd;

	if ((fd = net_accept (listen->fd, FALSE)) < 0)
	{
		GIFT_ERROR (("accept: %s", GIFT_STRERROR ()));
		return;
	}

	if (!(c = node_register (net_ip (net_peer_ip (fd)), -1, -1,
	                         NODE_NONE, FALSE)))
	{
		net_close (fd);
		return;
	}

	if (!conn_auth (c, FALSE) ||
	    FT_NODE(c)->state != NODE_DISCONNECTED || c->fd >= 0)
	{
		net_close (fd);
		return;
	}

	c->fd = fd;
	FT_NODE(c)->fd = fd;
	FT_NODE(c)->incoming = TRUE;

	node_state_set (c, NODE_CONNECTING);


	input_add (p, c, INPUT_WRITE,
			   (InputCallback) incoming_complete, TRUE);
}
