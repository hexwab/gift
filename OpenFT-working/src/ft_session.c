/*
 * $Id: ft_session.c,v 1.89 2003/12/23 21:29:39 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#include "ft_netorg.h"
#include "ft_conn.h"

#include "ft_search.h"
#include "ft_search_db.h"

#include "ft_session.h"

/*****************************************************************************/

/* do not turn this on unless you have surely lost your mind */
/* #define FT_SESSION_DEBUG */

/*****************************************************************************/

static BOOL session_auth_packet (TCPC *c, FTPacket *packet)
{
	ft_command_t cmd;
	BOOL         ret = FALSE;

	if (!packet)
		return FALSE;

	cmd = (ft_command_t)(ft_packet_command (packet));

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

static void handle_packet (TCPC *c, unsigned char *data, size_t len)
{
	FTPacket *packet;
	BOOL      ret = FALSE;

	packet = ft_packet_unserialize (data, len);

#ifdef FT_SESSION_DEBUG
	FT->DBGSOCK (FT, c, "%s", ft_packet_fmt (packet));
#endif /* FT_SESSION_DEBUG */

	if (session_auth_packet (c, packet))
		ret = ft_protocol_handle (c, packet);

	if (!ret)
	{
		FT->DBGSOCK (FT, c, "%i(0x%08x): failed %s", FT_SESSION(c)->stage,
		             (unsigned int)FT_NODE(c)->version, ft_packet_fmt (packet));

		ft_session_stop (c);
	}

	ft_packet_free (packet);
}

static void session_handle (int fd, input_id id, TCPC *c)
{
	unsigned char *data;
	size_t         data_len = 0;
	FDBuf         *buf;
	uint16_t       len;
	int            n;

	/* check for timeout */
	if (fd == -1 || id == 0)
	{
		ft_node_err (FT_NODE(c), FT_ERROR_TIMEOUT, NULL);
		ft_session_stop (c);

		return;
	}

	/* grab the data buffer */
	buf = tcp_readbuf (c);

	/* nb->flag, if non-zero, will indicate the total length of the data
	 * held within the OpenFT packet */
	len = buf->flag + FT_PACKET_HEADER;

	if ((n = fdbuf_fill (buf, len)) < 0)
	{
		char *errstr;

		switch (n)
		{
		 case FDBUF_EOF:   errstr = "EOF from socket";      break;
		 case FDBUF_NVAL:  errstr = "Invalid argument";     break;
		 case FDBUF_AGAIN: errstr = "Try again";            break;
		 case FDBUF_ERR:
		 default:          errstr = GIFT_NETERROR();        break;
		}

		ft_node_err (FT_NODE(c), FT_ERROR_UNKNOWN, errstr);
		ft_session_stop (c);

		return;
	}

	if (n > 0)
		return;

	data = fdbuf_data (buf, &data_len);
	assert (data != NULL);

	/* OpenFT packet length directly from the header */
	len = net_get16 (data, TRUE);

	if (buf->flag || len == 0)
	{
		/* reset */
		buf->flag = 0;
		fdbuf_release (buf);

		/* NOTE: we must call handle_packet after fdbuf_release as this may
		 * destroy the current connection, and thus buf along with it */
		handle_packet (c, data, data_len);
	}
	else if (buf->flag == 0)
	{
		/* we read a header block, adjust the flag hack so that we read
		 * the complete packet next time */
		buf->flag = len;
	}
}

/*****************************************************************************/

time_t ft_session_uptime (TCPC *c)
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

BOOL ft_session_start (TCPC *c, int incoming)
{
	assert (FT_NODE(c) != NULL);
	assert (FT_SESSION(c) != NULL);

	ft_node_set_state (FT_NODE(c), FT_NODE_CONNECTED);
	ft_session_stage (c, 0);

	input_add (c->fd, c, INPUT_READ,
	           (InputCallback)session_handle, TIMEOUT_DEF);

	return TRUE;
}

/*****************************************************************************/

static int send_packet (FTPacket *packet, TCPC *c)
{
	/* just a sanity check to make sure we dont add this entry right back to
	 * the session queue we're tryin to flush */
	assert (session_auth_packet (c, packet) == TRUE);

	/* ft_packet_send will takeover memory management */
	ft_packet_send (c, packet);
	return TRUE;
}

static void session_flush_queue (TCPC *c, int psend)
{
	FTPacket *packet;

	if (!c)
		return;

	while ((packet = array_shift (&(FT_SESSION(c)->pqueue))))
	{
#if 0
		FT->DBGSOCK (FT, c, "%i: flushing %s", psend, ft_packet_fmt (packet));
#endif

		if (!psend)
		{
			ft_packet_free (packet);
			continue;
		}

		send_packet (packet, c);
	}

	/*
	 * Deliver packets queued on this structure by ft_packet_sendto.  We
	 * don't want the same logic as above because we don't want to free the
	 * queued packets if this session didn't quite work on.  The idea is to
	 * persist the forwarding for a specified period of time, rather than a
	 * single try.
	 */
	if (psend && FT_NODE(c)->squeue)
	{
		FT->DBGSOCK (FT, c, "delivering buffered packet(s)...");

		while ((packet = array_shift (&(FT_NODE(c)->squeue))))
			send_packet (packet, c);

		array_unset (&(FT_NODE(c)->squeue));
	}

	/* TODO: should we disconnect if FT_PURPOSE_DELIVERY was the only
	 * purpose? */
	ft_session_remove_purpose (FT_NODE(c), FT_PURPOSE_DELIVERY);
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
	s->search_db = NULL;
	s->avail = 0;
	s->purpose = 0;
	s->child_eligibility = FALSE;
}

static void session_destroy_data (FTSession *s)
{
	/* flush packet write queue */
	session_flush_queue (s->c, FALSE);
	array_unset (&s->pqueue);

	/* clear the capabilities list */
	dataset_clear (s->cap);
	s->cap = NULL;

#if 0
	/* flush all the nodes we have written to this user so that we don't
	 * waste our local memory on someone that may not exist anymore */
	dataset_clear (s->nodelist);
	s->nodelist = NULL;
#endif

	/* make sure all the verify connections are dead */
	tcp_close_null (&s->verify_ft);
	tcp_close_null (&s->verify_http);
}

static void tidy_node_search (FTNode *node)
{
#if 0
	/* extremely awkward method of emulating a search reply from this user so
	 * that we dont infinitely hang if we indeed are waiting on this node
	 * for a search */
	ft_search_force_reply (NULL, node->ip);
#endif

	/* ensure that this node is not sharing anything to us (as our child) */
	ft_search_db_remove_host (node);

	/* im not entirely sure what this does...sorry */
	if (node->ninfo.klass & FT_NODE_SEARCH)
		ft_stats_remove_dep (node->ninfo.host);
}

static void tidy_node (FTNode *node)
{
	/* drop session-specific classes */
	ft_node_remove_class (node, FT_NODE_CHILD);
	ft_node_remove_class (node, FT_NODE_PARENT);

	/* TODO: this should do more, I think */
	tidy_node_search (node);
}

static void session_stop (TCPC *c)
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

void ft_session_stop (TCPC *c)
{
	FTNode *node;

	if (!c)
		return;

	node = FT_NODE(c);
	assert (node != NULL);

#if 0
	/* queue_flush prior to connection_close to eliminate a race condition in
	 * OpenFT's design */
	queue_flush (c);
#endif

	/* destroy all active _AFTER_ queue_flush as we may still be
	 * actively using one of these through an unflushed queue */
	ft_stream_clear_all (c);

	session_stop (c);
	tcp_close (c);

	node->session = NULL;

	/* eliminate a race condition when looping all nodes to destroy */
	if (openft->shutdown == FALSE)
		ft_node_set_state (node, FT_NODE_DISCONNECTED);
}

/*****************************************************************************/

static int handshake_timeout (TCPC *c)
{
	FT->DBGSOCK (FT, c, "handshaking took too long");

	/* drop the bastards */
	ft_session_stop (c);

	return FALSE;
}

static void stage_1 (TCPC *c)
{
	/* create a timer requiring that the session reach stage 4 before it
	 * is raised... */
	FT_SESSION(c)->start_timer =
		timer_add (2 * MINUTES, (TimerCallback) handshake_timeout, c);

	/* request that the remote node deliver their version to us */
	ft_packet_sendva (c, FT_VERSION_REQUEST, 0, NULL);
}

static void stage_2 (TCPC *c)
{
	FTPacket *pkt         = NULL;
	BOOL      need_search = FALSE;
	BOOL      need_index  = FALSE;

	/* exchange node capabilities */
	ft_packet_sendva (c, FT_NODECAP_REQUEST, 0, NULL);

	/* determine if we are in need of any additional node connections */
	need_search = BOOL_EXPR (ft_conn_need_parents() || ft_conn_need_peers());
	need_index  = BOOL_EXPR (ft_conn_need_index());

	/* construct a single nodelist request... */
	if (need_search || need_index)
	{
		pkt = ft_packet_new (FT_NODELIST_REQUEST, 0);

		if (need_search)
		{
			ft_packet_put_uint16 (pkt, (uint16_t)FT_NODE_SEARCH, TRUE);
			ft_packet_put_uint16 (pkt, 15, TRUE);
		}

		if (need_index)
		{
			ft_packet_put_uint16 (pkt, (uint16_t)FT_NODE_INDEX, TRUE);
			ft_packet_put_uint16 (pkt, 10, TRUE);
		}

		ft_packet_send (c, pkt);
	}
	else
	{
		/*
		 * If the only point of this connection was to gather nodes, and we
		 * no longer need a nodelist (hmm, thats curious?) then we should
		 * just abort before handshaking completes.
		 */
		if (ft_session_drop_purpose (FT_NODE(c), FT_PURPOSE_GET_NODES))
			return;
	}

	/* always attempt to update our node information */
	ft_packet_sendva (c, FT_NODEINFO_REQUEST, 0, NULL);
}

static void stage_3 (TCPC *c)
{
	/* notify the remote node that we have finished port verification and are
	 * ready to move to stage 4 whenever they are */
	ft_packet_sendva (c, FT_SESSION_REQUEST, 0, NULL);
}

static void stage_4 (TCPC *c)
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

void ft_session_stage (TCPC *c, unsigned char current)
{
	if (!c)
		return;

	if (FT_SESSION(c)->stage != current)
		return;

	FT_SESSION(c)->stage++;

#ifdef FT_SESSION_DEBUG
	FT->DBGSOCK (FT, c, "stage = %i", (int)FT_SESSION(c)->stage);
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

int ft_session_queue (TCPC *c, FTPacket *packet)
{
	if (!c || !FT_SESSION(c) || !packet)
		return FALSE;

	/* this packet is authorized to go out at this time, deliver it now */
	if (session_auth_packet (c, packet))
		return FALSE;

#if 0
	FT->DBGSOCK (FT, c, "queueing %s", ft_packet_fmt (packet));
#endif

	if (!array_push (&(FT_SESSION(c)->pqueue), packet))
		return FALSE;

	return TRUE;
}

/*****************************************************************************/

static void connect_complete (TCPC *c, int fd, input_id id, int incoming)
{
	char *err = NULL;

	assert (FT_CONN(FT_NODE(c)) == c);

	assert (tcp_flush (c, TRUE) == 0);
	assert (c->wqueue_id == 0);
	input_remove_all (c->fd);

	/* special exception used by input_add when the timer has been raised
	 * and the socket has already been closed */
	if (fd == -1 || id == 0)
	{
		/* ensure both conditions exist together */
		assert (fd == -1);
		assert (id == 0);

		err = "Connection timed out";
	}
	else if (net_sock_error (c->fd))
	{
		/* pass the strerror through */
		err = stringf ("Socket error: %s", GIFT_NETERROR());
	}

	/* one of the two above error conditions occurred, set the err on the
	 * object and disconnect */
	if (err)
	{
		/* unknown err is not entirely accurate :) */
		ft_node_err (FT_NODE(c), FT_ERROR_UNKNOWN, err);
		ft_session_stop (c);
		return;
	}

	if (!ft_session_start (c, incoming))
		ft_session_stop (c);
}

static void outgoing_complete (int fd, input_id id, TCPC *c)
{
	connect_complete (c, fd, id, FALSE);
}

int ft_session_connect (FTNode *node, ft_purpose_t goal)
{
	FTSession  *session;
	TCPC       *c;

	/* already connected, fail */
	if (FT_CONN(node))
	{
		assert (FT_CONN(node)->fd >= 0);
		FT->DBGFN (FT, "%s: already connected", ft_node_fmt (node));

		ft_session_add_purpose (node, goal);
		return -1;
	}

	if (ft_node_fw (node) || !ft_conn_auth (node, TRUE))
	{
		FT->DBGFN (FT, "%s: fw/unauth", ft_node_fmt (node));
		return -1;
	}

	FT->DBGFN (FT, "attempting connection to %s", ft_node_fmt (node));

	/* attempt the connection before we actually create the session as session
	 * instantiation implies more things than a simple connection */
	if (!(c = tcp_open (node->ninfo.host, node->ninfo.port_openft, FALSE)))
	{
		FT->err (FT, "unable to connect to %s: %s", ft_node_fmt (node),
		         GIFT_NETERROR());
		return -1;
	}

	/* create the session object (as well as the necessary connection object)
	 * so that we may assign the fd and track this node's status through
	 * the node structure (NOTE: create_session is returning a pointer to
	 * data it has actually assigned to node->session) */
	if (!(session = create_session (node)))
	{
		tcp_close (c);
		return -1;
	}

	/* if we have open children slots, try not to use the purpose system
	 * for own outgoing connections (this implies that we are a search node) */
	if (ft_conn_children_left() > 0)
	{
		/* just keep this connection alive until the main idle disconnection
		 * loop */
		goal |= FT_PURPOSE_UNDEFINED;
	}

	/* new connection, initialize purpose */
	ft_session_set_purpose (node, goal);

	/* update the node structure to reflect this new connection attempt */
	session->c = c;
	session->incoming = FALSE;
	ft_node_set_state (node, FT_NODE_CONNECTING);
	c->udata = node;

	input_add (c->fd, c, INPUT_WRITE,
			   (InputCallback)outgoing_complete, TIMEOUT_DEF);

	return c->fd;
}

static void incoming_complete (int fd, input_id id, TCPC *c)
{
	connect_complete (c, fd, id, TRUE);
}

void ft_session_incoming (int fd, input_id id, TCPC *listen)
{
	TCPC       *c;
	FTNode     *node;
	FTSession  *session;

	openft->ninfo.indirect = 0;

	if (!(c = tcp_accept (listen, FALSE)))
	{
		FT->err (FT, "accept: %s", GIFT_NETERROR());
		return;
	}

	if (!(node = ft_node_register (c->host)) || FT_CONN(node))
	{
		tcp_close (c);
		return;
	}

	/* see the documentation above in ::ft_session_connect */
	if (!ft_conn_auth (node, FALSE) || !(session = create_session (node)))
	{
		/* yes, i realize that we're not unregistering the node, this is
		 * because it is quite possible that this node will connect again
		 * soon and we should keep information about them if possible */
		tcp_close (c);
		return;
	}

	ft_session_set_purpose (node, FT_PURPOSE_DRIFTER);

	session->c = c;
	session->incoming = TRUE;
	ft_node_set_state (node, FT_NODE_CONNECTING);
	c->udata = node;

	input_add (c->fd, c, INPUT_WRITE,
	           (InputCallback)incoming_complete, TIMEOUT_DEF);
}

/*****************************************************************************/

void ft_session_set_purpose (FTNode *node, ft_purpose_t goal)
{
	if (!node || !node->session)
		return;

#if 0
	if (goal == 0)
		FT->DBGSOCK (FT, FT_CONN(node), "warning: setting undefined purpose");
#endif

	node->session->purpose = goal;
}

void ft_session_add_purpose (FTNode *node, ft_purpose_t goal)
{
	if (!node || !node->session)
		return;

	node->session->purpose |= goal;
}

ft_purpose_t ft_session_remove_purpose (FTNode *node, ft_purpose_t goal)
{
	if (!node || !node->session)
		return 0;

	node->session->purpose &= ~goal;
	return node->session->purpose;
}

BOOL ft_session_drop_purpose (FTNode *node, ft_purpose_t goal)
{
	ft_purpose_t old_goal;
	ft_purpose_t new_goal;
	BOOL         allow_drop;

	assert (node != NULL);
	assert (node->session != NULL);

	/* make sure tihs system doesn't work against us */
	if (node->ninfo.klass & FT_NODE_SEARCH)
	{
		if (ft_conn_need_parents() == TRUE)
			ft_session_add_purpose (node, FT_PURPOSE_PARENT_TRY);

		if (ft_conn_need_peers() == TRUE)
			ft_session_add_purpose (node, FT_PURPOSE_PEER_KEEP);
	}

	if (node->ninfo.klass & FT_NODE_INDEX)
	{
		if (ft_conn_need_index() == TRUE)
			ft_session_add_purpose (node, FT_PURPOSE_UNDEFINED);
	}

	if (node->ninfo.klass & FT_NODE_PARENT)
		ft_session_add_purpose (node, FT_PURPOSE_PARENT_KEEP);

	if (node->ninfo.klass & FT_NODE_CHILD)
		ft_session_add_purpose (node, FT_PURPOSE_UNDEFINED);

	/*
	 * Refuse to drop the connection if the purpose never originally was
	 * defined to do what we are now removing.  This is a hack to work-around
	 * limited functional usage of this new system.
	 */
	old_goal = node->session->purpose;
	allow_drop = BOOL_EXPR (old_goal & goal);

	/* remove the described purpose and check that we have no meaning
	 * or reason for this session any longer */
	if ((new_goal = ft_session_remove_purpose (node, goal)) == 0 && allow_drop)
	{
		/* go ahead... */
		ft_node_err (node, FT_ERROR_IDLE,
		             stringf ("%hu: Purpose completed", (unsigned short)goal));
		ft_session_stop (FT_CONN(node));

		return TRUE;
	}

	return FALSE;
}
