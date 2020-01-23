/*
 * $Id: as_session_man.c,v 1.35 2005/09/15 21:13:53 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static as_bool maintenance_timer_func (ASSessMan *man);

static as_bool sessman_maintain (ASSessMan *man);

static as_bool session_state_cb (ASSession *session, ASSessionState state);

static as_bool session_packet_cb (ASSession *session, ASPacketType type,
                                  ASPacket *packet);

/*****************************************************************************/

/* allocate and init session manager */
ASSessMan *as_sessman_create ()
{
	ASSessMan *man;

	if (!(man = malloc (sizeof (ASSessMan))))
		return NULL;

	man->connections = 0;
	man->connecting = NULL;
	man->connected = NULL;

	man->maintenance_timer = timer_add (2*MINUTES,
	                                    (TimerCallback)maintenance_timer_func,
	                                    man);

	return man;
}

/* free manager */
void as_sessman_free (ASSessMan *man)
{
	if (!man)
		return;

	timer_remove (man->maintenance_timer);

	/* disconnect everything */
	as_sessman_connect (man, 0);

	free (man);
}

/*****************************************************************************/

/* Returns number of actually established sessions */
unsigned int as_sessman_established (ASSessMan *man)
{
	return list_length (man->connected);
}

/*****************************************************************************/

/* Set number of sessions that should be maintained at all times. Setting this
 * to zero will disconnect from the network. Anything non-zero will start
 * connecting.
 */
void as_sessman_connect (ASSessMan *man, unsigned int connections)
{
	man->connections = connections;

	AS_DBG_3 ("Requested: %d, connected: %d, connecting: %d",
	          man->connections, list_length (man->connected), 
	          list_length (man->connecting));

	sessman_maintain (man);
}

/*****************************************************************************/

/* Calls func for each established session. Do not remove or add sessions
 * during iteration (e.g. don't call as_sessman_connect). Returns number of
 * times func returned TRUE.
 */
int as_sessman_foreach (ASSessMan *man, ASSessionForeachFunc func,
                        void *udata)
{
	List *l;
	int count = 0;
	
	for (l = man->connected; l; l = l->next)
		if (func (l->data, udata))
			count++;
	
	return count;
}

/*****************************************************************************/

/* This timer is called periodically to make sure the number of active
 * sessions is at the requested level. It can happen for example that the
 * node cache temporarily returns no nodes because they have been tried too
 * recently. Without this timer it can happen that sessman_maintain is never
 * called again in this case.
 */
static as_bool maintenance_timer_func (ASSessMan *man)
{
	sessman_maintain (man);
		
	return TRUE; /* Reset timer. */
}

static int sessman_disconnect_itr (ASSession *session, ASSessMan *man)
{
	as_session_disconnect (session, FALSE);
	/* notify node manager */
	as_nodeman_update_disconnected (AS->nodeman, session->host);
	as_session_free (session);

	return TRUE; /* remove link */
}

/* take necessary steps to maintain man->connections sessions */
static as_bool sessman_maintain (ASSessMan *man)
{
	ASSession *session;
	unsigned int connected = list_length (man->connected);
	unsigned int connecting = list_length (man->connecting);
	int len;
		
	if (man->connections == 0)
	{
		/* disconnect everything */
		man->connecting = list_foreach_remove (man->connecting,
		                     (ListForeachFunc)sessman_disconnect_itr, man);
		man->connected = list_foreach_remove (man->connected,
		                     (ListForeachFunc)sessman_disconnect_itr, man);
	}
	else if (man->connections <= connected)
	{
		/* We have more connections than needed. First stop all discovery. */
		man->connecting = list_foreach_remove (man->connecting,
		                     (ListForeachFunc)sessman_disconnect_itr, man);

		/* Now remove superfluous connections.
		 * TODO: Be smart about which connections we remove.
		 */
		len = connected - man->connections;
		
		while (len > 0)
		{
			session = (ASSession *) man->connected->data;

			as_session_disconnect (session, FALSE);
			/* notify node manager */
			as_nodeman_update_disconnected (AS->nodeman, session->host);
			as_session_free (session);

			man->connected = list_remove_link (man->connected, man->connected);
			len--;
		}
	}
	else if (man->connections > connected)
	{
		/* We need more connections. Fill up discovery queue. */
		len = AS_SESSION_PARALLEL_ATTEMPTS - connecting;

		while (len > 0)
		{
			ASSession *session;
			ASNode *node;

			/* Get next node */
			if (!(node = as_nodeman_next (AS->nodeman)))
			{
				/* FIXME: Use Ares http cache by adding download code to
				 * node manager and calling it from here.
				 */
				AS_ERR ("Ran out of nodes");
				return FALSE;	
			}

			/* Create session */
			if (!(session = as_session_create (session_state_cb,
			                                   session_packet_cb)))
			{
				AS_ERR ("Insufficient memory");
				as_nodeman_update_failed (AS->nodeman, node->host);
				return FALSE; /* hmm */
			}

			session->udata = man;

#if 0
			AS_HEAVY_DBG_3 ("Trying node %s:%d, weight: %.02f",
			                net_ip_str (node->host), node->port, node->weight);
#endif

			/* Connect to node */
			if (!(as_session_connect (session, node->host, node->port)))
			{
				as_nodeman_update_failed (AS->nodeman, node->host);
				as_session_free (session);
				continue; /* try next node */
			}

			/* Add session to connecting list */
			man->connecting = list_prepend (man->connecting, session);
			len--;
		}
	}

	connected = list_length (man->connected);
	connecting = list_length (man->connecting);

	AS_HEAVY_DBG_3 ("session_maintain: requested: %d, connected: %d, connecting: %d",
	                man->connections, connected, connecting);

	/* Let NetInfo know what's going on */
	as_netinfo_handle_connect (AS->netinfo, man->connections, connected);

	return TRUE;
}

/*****************************************************************************/

static as_bool session_state_cb (ASSession *session, ASSessionState state)
{
	ASSessMan *man = (ASSessMan*) session->udata;
	as_bool ret;

	switch (state)
	{
	case SESSION_DISCONNECTED:
		AS_DBG_2 ("DISCONNECTED %s:%d",
		          net_ip_str (session->host), session->port);

		/* notify node manager */
		as_nodeman_update_disconnected (AS->nodeman, session->host);

		/* remove from list and free session */
		man->connected = list_remove (man->connected, session);
		as_session_free (session);

		/* keep things running */
		sessman_maintain (man);

		return FALSE;

	case SESSION_FAILED:
		AS_HEAVY_DBG_2 ("FAILED %s:%d",
		                net_ip_str (session->host), session->port);

		/* notify node manager */
		as_nodeman_update_failed (AS->nodeman, session->host);

		/* remove from list and free session */
		man->connecting = list_remove (man->connecting, session);
		as_session_free (session);

		/* keep things running */
		sessman_maintain (man);

		return FALSE;

	case SESSION_CONNECTING:
		AS_HEAVY_DBG_2 ("CONNECTING %s:%d",
		                net_ip_str (session->host), session->port);
		break;

	case SESSION_HANDSHAKING:
		AS_HEAVY_DBG_2 ("HANDSHAKING %s:%d",
		                net_ip_str (session->host), session->port);
		break;

	case SESSION_CONNECTED:
		AS_DBG_2 ("CONNECTED %s:%d",
		          net_ip_str (session->host), session->port);
		
		/* notify node manager */
		as_nodeman_update_connected (AS->nodeman, session->host);

		/* remove from connecting list */
		man->connecting = list_remove (man->connecting, session);

		if (list_length (man->connected) < (int) man->connections)
		{
			/* add session to connected list */
			man->connected = list_prepend (man->connected, session);

			/* send queued searches to new session */
			as_searchman_new_session (AS->searchman, session);

			/* submit shares to new supernode */
			as_shareman_submit (AS->shareman, session);

			ret = TRUE;
		}
		else
		{
			/* Immediately disconnect since we already have enough
			 * connections. Note that we cannot use sessman_maintain here 
			 * because it doesn't tell us whether _this_ connection was 
			 * removed which we would have to signal to callback raiser.
			 */
			as_session_disconnect (session, FALSE);
			/* notify node manager */
			as_nodeman_update_disconnected (AS->nodeman, session->host);
			as_session_free (session);
			ret = FALSE; /* session was freed */
		}

		/* Now that we have removed the this session if necessary we can safely
		 * call session_maintain to abort other pending connects if necessary.
		 */
		sessman_maintain (man);

#if 0
		AS_DBG_3 ("Session status: requested %d, connected: %d, connecting: %d",
	              man->connections, list_length (man->connected), 
	              list_length (man->connecting));
#endif

		return ret;
	}

	return TRUE;
}

static as_bool session_packet_cb (ASSession *session, ASPacketType type,
                                  ASPacket *packet)
{
	/* FIXME: passing this one level up to ASInstance and dispatch it there to
	 * the different managers would make more sense.
	 */

	switch (type)
	{
	case PACKET_LOCALIP:
		as_netinfo_handle_ip (AS->netinfo, session, packet);
		break;

	case PACKET_STATS:
	case PACKET_STATS2:
		as_netinfo_handle_stats (AS->netinfo, session, packet);
		break;

	case PACKET_RESULT:
		as_searchman_result (AS->searchman, session, packet);
		break;

	case PACKET_NICKNAME:
		as_netinfo_handle_nick (AS->netinfo, session, packet);
		break;

	case PACKET_NODELIST:
	case PACKET_NODELIST2:
		/* FIXME */
		break;
	case PACKET_SUPERINFO:
		/* FIXME */
		break;
	case PACKET_PUSH:
		as_pushreplyman_handle (AS->pushreplyman, packet);
		break;
	default:
		AS_WARN_2 ("Got unknown packet 0x%02x from %s:",
			   type, net_ip_str (session->host));
		as_packet_dump (packet);
	}

	return TRUE;
}

/*****************************************************************************/

