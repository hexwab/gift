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
#include "ft_conn.h"
#include "ft_search.h"

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
		ft_node_err (FT_NODE(c), FT_ERROR_UNKNOWN, GIFT_NETERROR());
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

	if (!c || !FT_SESSION(c))
		return 0;

	start = FT_SESSION(c)->start;
	curr  = time (NULL);

	if (!start)
		return 0;

	return MAX (0, curr - start);
}

/*****************************************************************************/

static FTSession *create_session (FTNode *node)
{
	FTSession *s;

	if (!node)
		return NULL;

	/* even if we already have a session, we still want to memset everything */
	if (!(s = node->session))
	{
		if (!(s = malloc (sizeof (FTSession))))
			return NULL;
	}

	memset (s, 0, sizeof (FTSession));
	node->session = s;

	return s;
}

int ft_session_start (Connection *c, int incoming)
{
	assert (FT_NODE(c) != NULL);
	assert (FT_SESSION(c) != NULL);

	ft_node_set_state (FT_NODE(c), NODE_CONNECTED);
	ft_session_stage (c, 0);

	input_add (openft_p, c, INPUT_READ,
	           (InputCallback)session_handle, TRUE);

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

static void session_reset_data (FTSession *s)
{
	/* remove the handshaking timeout */
	timer_remove (s->start_timer);
	s->start_timer = 0;

	/* flush everything else */
	memset (&s->stats, 0, sizeof (s->stats));
	s->stage = 0;
	s->start = 0;
	s->verified = FALSE;
	s->heartbeat = 0;
}

static void session_destroy_data (FTSession *s)
{
	/* flush packet write queue */
	session_flush_queue (s->c, FALSE);
	list_queue_free (s->queue);
	s->queue = NULL;

	/* destroy all active FTStream's */
	ft_stream_clear_all (s->c);

	/* clear the capabilities list */
	dataset_clear (s->cap);
	s->cap = NULL;

	/* flush all the nodes we have written to this user so that we don't
	 * waste our local memory on someone that may not exist anymore */
	dataset_clear (s->nodelist);
	s->nodelist = NULL;

	/* make sure all the verify connections are dead */
	connection_close_null (&s->verify_openft);
	connection_close_null (&s->verify_http);
}

static void tidy_node_search (FTNode *node)
{
	/* extremely awkward method of emulating a search reply from this user so
	 * that we dont infinitely hang if we indeed are waiting on this node
	 * for a search */
	ft_search_force_reply (NULL, node->ip);

	/* ensure that this node is not sharing anything to us (as our child) */
	ft_shost_remove (node->ip);

	/* im not entirely sure what this does...sorry */
	if (node->klass & NODE_SEARCH)
		ft_stats_remove (node->ip, 0);
}

static void tidy_node (FTNode *node)
{
	ft_node_remove_class (node, NODE_CHILD);
	ft_node_remove_class (node, NODE_PARENT);

	/* TODO: this should do more, I think */
	tidy_node_search (node);
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

	/* cleanup all node-related session data, mostly because we are still
	 * transitioning the data structures over to the new
	 * session/connection/node model */
	tidy_node (FT_NODE(c));

	/* memory management of all the runtime data */
	session_reset_data (FT_SESSION(c));
	session_destroy_data (FT_SESSION(c));

	/* free, this may change in the future */
	free (FT_SESSION(c));
	FT_NODE(c)->session = NULL;
}

void ft_session_stop (Connection *c)
{
	FTNode *node;

	if (!c)
		return;

	node = FT_NODE(c);
	assert (node != NULL);

	session_stop (c);
	connection_close (c);

	node->session = NULL;
	ft_node_set_state (node, NODE_DISCONNECTED);
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
	assert (FT_CONN(FT_NODE(c)) == c);

	input_remove (c);

	if (net_sock_error (c->fd))
	{
		ft_node_err (FT_NODE(c), FT_ERROR_UNKNOWN, GIFT_NETERROR());
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

int ft_session_connect (FTNode *node)
{
	FTSession  *session;
	Connection *c;

	TRACE_FUNC();

	/* already connected, fail */
	if (FT_CONN(node))
	{
		assert (FT_CONN(node)->fd >= 0);
		TRACE(("%s",ft_node_fmt(node)));
		return -1;
	}

	if (ft_node_fw (node) || !ft_conn_auth (node, TRUE)) {
		TRACE(("%s",ft_node_fmt(node)));
		return -1;
	}

	TRACE (("attempting connection to %s", ft_node_fmt (node)));

	/* attempt the connection before we actually create the session as session
	 * instantiation implies more things than a simple connection */
	if (!(c = connection_open (openft_p, net_ip_str (node->ip), node->port,
	                           FALSE)))
	{
		GIFT_ERROR (("unable to connect to %s: %s", ft_node_fmt (node),
		             GIFT_NETERROR()));
		return -1;
	}

	/* create the session object (as well as the necessary connection object)
	 * so that we may assign the fd and track this node's status through
	 * the node structure (NOTE: create_session is returning a pointer to
	 * data it has actually assigned to node->session) */
	if (!(session = create_session (node)))
	{
		connection_close (c);
		return -1;
	}

	/* update the node structure to reflect this new connection attempt */
	session->c = c;
	session->incoming = FALSE;
	ft_node_set_state (node, NODE_CONNECTING);
	c->data = node;

	input_add (openft_p, c, INPUT_WRITE,
			   (InputCallback)outgoing_complete, TRUE);

	return c->fd;
}

static void incoming_complete (Protocol *p, Connection *c)
{
	connect_complete (c, TRUE);
}

void ft_session_incoming (Protocol *p, Connection *listen)
{
	Connection *c;
	FTNode     *node;
	FTSession  *session;

	if (!(c = connection_accept (openft_p, listen, FALSE)))
	{
		GIFT_ERROR (("accept: %s", GIFT_NETERROR()));
		return;
	}

	if (!(node = ft_node_register (net_peer (c->fd))) || FT_CONN(node))
	{
		connection_close (c);
		return;
	}

	/* see the documentation above in ::ft_session_connect */
	if (!ft_conn_auth (node, FALSE) || !(session = create_session (node)))
	{
		/* yes, i realize that we're not unregistering the node, this is
		 * because it is quite possible that this node will connect again
		 * soon and we should keep information about them if possible */
		connection_close (c);
		return;
	}

	session->c = c;
	session->incoming = TRUE;
	ft_node_set_state (node, NODE_CONNECTING);
	c->data = node;

	input_add (p, c, INPUT_WRITE,
	           (InputCallback)incoming_complete, TRUE);
}
