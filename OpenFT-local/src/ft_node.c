/*
 * $Id: ft_node.c,v 1.61 2005/03/24 22:08:56 hexwab Exp $
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

#include "ft_search_db.h"
#include "ft_netorg.h"
#include "ft_conn.h"

#include "ft_node.h"

/*****************************************************************************/

FTNode *ft_node_new (in_addr_t ip)
{
	FTNode *node;

	if (!(node = MALLOC (sizeof (FTNode))))
		return NULL;

	node->ninfo.host = ip;
	node->ninfo.klass = FT_NODE_USER;
	node->ninfo.indirect = TRUE;
	node->state = FT_NODE_DISCONNECTED;

	return node;
}

static void flush_queue (Array **queue)
{
	FTPacket *packet;

	while ((packet = array_shift (queue)))
		ft_packet_free (packet);

	array_unset (queue);
}

void ft_node_free (FTNode *node)
{
	if (!node)
		return;

	/* this will destroy all the necessary associations for us */
	if (node->session)
	{
		if (node->session->search_db && openft->shutdown == FALSE)
			FT->warn (FT, "removing node with an active search database!");

		ft_session_stop (FT_CONN(node));
	}

	free (node->ninfo.alias);
	flush_queue (&node->squeue);

	free (node);
}

/*****************************************************************************/

FTNode *ft_node_register (in_addr_t ip)
{
	return ft_node_register_full (ip, 0, 0, 0, 0, 0, 0);
}

FTNode *ft_node_register_full (in_addr_t ip,
                               in_port_t port,
                               in_port_t http_port,
                               ft_class_t klass,
                               time_t vitality, time_t uptime, uint32_t ver)
{
	FTNode *node;

	if ((node = ft_netorg_lookup (ip)))
		return node;

	if (!(node = ft_node_new (ip)))
		return NULL;

	ft_node_set_port      (node, port);
	ft_node_set_http_port (node, http_port);
	ft_node_set_class     (node, klass);

	node->last_session = vitality;
	node->uptime       = uptime;
	node->version      = ver;

	/* TODO: we need to cap the size of this structure so that we don't end
	 * up adding thousands of disconnected and generally useless nodes */
	ft_netorg_add (node);
	return node;
}

void ft_node_unregister (FTNode *node)
{
	if (!node)
		return;

	ft_netorg_remove (node);
	ft_node_free (node);
}

void ft_node_unregister_ip (in_addr_t ip)
{
	ft_node_unregister (ft_netorg_lookup (ip));
}

/*****************************************************************************/

void ft_node_err (FTNode *node, ft_error_t err, const char *errtxt)
{
	if (!node)
		return;

	node->lasterr = err;

	free (node->lasterr_msg);
	node->lasterr_msg = STRDUP (errtxt);
}

char *ft_node_geterr (FTNode *node)
{
	static char errbuf[128];
	char *family = NULL;

	assert (node != NULL);

	switch (node->lasterr)
	{
	 case FT_ERROR_SUCCESS:     family = "";                      break;
	 case FT_ERROR_IDLE:        family = "Idle: ";                break;
	 case FT_ERROR_TIMEOUT:     family = "Connection timed out";  break;
	 case FT_ERROR_VERMISMATCH: family = "VerMismatch: ";         break;
	 case FT_ERROR_UNKNOWN:     family = "";                      break;
	}

	assert (family != NULL);

	snprintf (errbuf, sizeof (errbuf) - 1, "%s%s",
	          STRING_NOTNULL(family),
	          STRING_NOTNULL(node->lasterr_msg));

	/* reset the error (TODO: use ft_node_err more often so we dont really
	 * have to do this) */
	ft_node_err (node, FT_ERROR_SUCCESS, NULL);

	return errbuf;
}

/*****************************************************************************/

static void node_set_indirect (FTNode *node)
{
	in_port_t openft, http;

	openft = node->ninfo.port_openft;
	http   = node->ninfo.port_http;

	node->ninfo.indirect = BOOL_EXPR (openft == 0);
}

void ft_node_set_port (FTNode *node, in_port_t port)
{
	if (!node)
		return;

	node->ninfo.port_openft = port;
	node_set_indirect (node);

	/* search and index nodes are not allowed to be firewalled, remove the
	 * class */
	if (port == 0 && (node->ninfo.klass & (FT_NODE_SEARCH | FT_NODE_INDEX)))
	{
		ft_node_remove_class (node, FT_NODE_SEARCH);
		ft_node_remove_class (node, FT_NODE_INDEX);
	}
}

void ft_node_set_http_port (FTNode *node, in_port_t http_port)
{
	if (!node || !http_port)
		return;

	node->ninfo.port_http = http_port;
	node_set_indirect (node);
}

static BOOL is_valid_alias (const char *alias)
{
	const char *ptr;
	size_t len;

	if (alias == NULL)
		return FALSE;

	len = strlen (alias);

	if (len == 0 || len > 32)
		return FALSE;

	/*
	 * Check for any invalid characters.  We should really parse the invalid
	 * characters out instead of rejecting the alias.
	 */
	for (ptr = alias; *ptr; ptr++)
	{
		/* alias@ipaddr */
		if (*ptr == '@')
			return FALSE;
	}

	return TRUE;
}

char *ft_node_set_alias (FTNode *node, const char *alias)
{
	if (node == NULL)
		return NULL;

	/* destroy the previously installed node alias */
	free (node->ninfo.alias);

	/*
	 * Apply alias restriction rules imposed by the protocol.  Please note
	 * that the protocol actually allows us to remove the offending
	 * characters and/or clamp the strings length if not appropriate.  We are
	 * merely voiding the entire alias if anything that violates the rules is
	 * found.
	 */
	if (is_valid_alias (alias) == FALSE)
		alias = NULL;

	node->ninfo.alias = STRDUP (alias);

	return node->ninfo.alias;
}

/*****************************************************************************/

void ft_node_queue (FTNode *node, FTPacket *packet)
{
	if (!node || !packet)
		return;

	assert (FT_CONN(node) == NULL);

	if (!array_push (&node->squeue, packet))
	{
		FT->warn (FT, "unable to queue %s: %s", ft_packet_fmt (packet),
		          GIFT_STRERROR());
	}
}

/*****************************************************************************/

/*
 * Sigh, this is a terribly misleading function.  Basically, it selects the
 * most appropriate human-readable string given the class, however given the
 * way classes are stored in OpenFT this may or may not leave out vital
 * pieces of information without any mention of it.  Eventually I would like
 * to have a function that has a return value that reflects all classes and
 * modifiers.
 */
char *ft_node_classstr (ft_class_t klass)
{
	char *str;

	if (klass & FT_NODE_INDEX)
		str = "INDEX";
	else if (klass & FT_NODE_PARENT)
		str = "PARENT";
	else if (klass & FT_NODE_SEARCH)
		str = "SEARCH";
	else if (klass & FT_NODE_CHILD)
		str = "CHILD";
	else if (klass & FT_NODE_USER)
		str = "USER";
	else
		str = "NONE";

	return str;
}

static void add_class (String *s, int *first, char *str)
{
	if (*first == FALSE)
		string_append (s, " | ");
	else
		*first = FALSE;

	string_append (s, str);
}

char *ft_node_classstr_full (ft_class_t klass)
{
	static char buf[128];
	String     *s;
	int         first = TRUE;

	if (!(s = string_new (buf, sizeof (buf), 0, FALSE)))
		return NULL;

	if (klass & FT_NODE_INDEX)
		add_class (s, &first, "INDEX");

	if (klass & FT_NODE_SEARCH)
		add_class (s, &first, "SEARCH");

	if (klass & FT_NODE_USER)
		add_class (s, &first, "USER");

	if (klass & FT_NODE_PARENT)
		add_class (s, &first, "PARENT");

	if (klass & FT_NODE_CHILD)
		add_class (s, &first, "CHILD");

	return string_free_keep (s);
}

/*
 * Return the appropriate state string to be displayed in a human-readable
 * fashion.  Unlike class_str, this function does not need to sacrifice
 * information, and thus is more acceptable.
 */
char *ft_node_statestr (ft_state_t state)
{
	char *str;

	switch (state)
	{
	 case FT_NODE_DISCONNECTED:
		str = "DISCO";
		break;
	 case FT_NODE_CONNECTED:
		str = "FINAL";
		break;
	 case FT_NODE_CONNECTING:
		str = "LIMBO";
		break;
	 default:
		str = "UNKNOWN";
		break;
	}

	return str;
}

/*****************************************************************************/

static void handle_state_change (FTNode *node, ft_state_t orig,
                                 ft_state_t now)
{
	/* notify the network organization code that a change has occurred and
	 * should be reflected in the appropriate data structures */
	ft_netorg_change (node, node->ninfo.klass, orig);

	/* log this state change */
	if (now != FT_NODE_CONNECTING &&
	    now != FT_NODE_CONNECTED &&
	    now != FT_NODE_DISCONNECTED)
	{
		FT->dbg (FT, "%s (%s) -> %s: %s",
		         ft_node_fmt (node),
		         ft_node_classstr (node->ninfo.klass), ft_node_statestr (now),
		         ft_node_geterr (node));
	}
}

void ft_node_set_state (FTNode *node, ft_state_t state)
{
	ft_state_t orig;

	if (!node)
		return;

	/* clamp input */
	state &= FT_NODE_STATE_MASK;
	assert (state != 0);

	/*
	 * When a state is changed, we must first identify what has changed for
	 * the logic here to do anything useful.  More importantly, we need to
	 * identify that -something- has changed as it is perfectly valid for a
	 * disconnected node to be set disconnected again.
	 */
	if ((orig = node->state) == state)
		return;

	/* set the state early so that any subsequent (foreign) logic doesn't
	 * flip out */
	node->state = state;

	/* write to the log file that something happened, for mostly diagnostic
	 * purposes */
	handle_state_change (node, orig, state);
}

/*****************************************************************************/

static int submit_to_index (FTNode *node, ft_nodeinfo_t *ninfo)
{
	FTPacket *pkt;

	if (!(pkt = ft_packet_new (FT_STATS_DIGEST_REMOVE, 0)))
		return FALSE;

	ft_packet_put_ip (pkt, ninfo->host);

	return BOOL_EXPR (ft_packet_send (FT_CONN(node), pkt) >= 0);
}

static void handle_class_loss (FTNode *node, ft_class_t orig, ft_class_t loss)
{
	if (loss & FT_NODE_PARENT)
	{
		/* gracefully unshare all files with the remote node if we detect that
		 * the parent modifier must be removed */
		ft_packet_sendva (FT_CONN(node), FT_SHARE_REMOVE_REQUEST, 0, NULL);

		/* remove the parent purpose...should we use drop_purpose here? */
		if (ft_session_remove_purpose (node, FT_PURPOSE_PARENT_KEEP) == 0)
			FT->DBGSOCK (FT, FT_CONN(node), "no purpose left, what to do?");
	}

	if (loss & FT_NODE_CHILD)
	{
		/* when a child is lost, all shares that were previously indexed by this
		 * user must be invalidated with the index node for stats purposes */
		ft_netorg_foreach (FT_NODE_INDEX, FT_NODE_CONNECTED, 0,
		                   FT_NETORG_FOREACH(submit_to_index), &node->ninfo.host);
	}
}

static BOOL submit_digest_child (FTNode *child, FTNode *index)
{
	FTPacket *pkt;

	/* children that have not yet begun syncing their shares will not have
	 * this set, so leave them out of the digest */
	if (!FT_SEARCH_DB(child))
		return FALSE;

	if (!(pkt = ft_packet_new (FT_STATS_DIGEST_ADD, 0)))
		return FALSE;

	ft_packet_put_ip     (pkt, child->ninfo.host);
	ft_packet_put_uint32 (pkt, (uint32_t)FT_SEARCH_DB(child)->shares, TRUE);
	ft_packet_put_uint32 (pkt, (uint32_t)FT_SEARCH_DB(child)->size,   TRUE);

	ft_packet_send (FT_CONN(index), pkt);

	return TRUE;
}

/* HACK: Temporarily exported for ft_handshake.c:ft_nodeinfo_response. */
void handle_class_gain (FTNode *node, ft_class_t orig, ft_class_t gain)
{
	/* if we have a new index || parent node, ask for a current stats digest
	 * update from this node */
	if (gain & (FT_NODE_INDEX | FT_NODE_PARENT))
		ft_packet_sendva (FT_CONN(node), FT_STATS_REQUEST, 0, NULL);

	/*
	 * If we are a search node and we get a new INDEX node connection,
	 * send a complete digest report.
	 */
	if (openft->ninfo.klass & FT_NODE_SEARCH && gain & FT_NODE_INDEX)
	{
		ft_netorg_foreach (FT_NODE_CHILD, FT_NODE_CONNECTED, 0,
		                   FT_NETORG_FOREACH(submit_digest_child), node);
	}

	if (gain & FT_NODE_SEARCH)
	{
		/*
		 * New search nodes should be contacted for parenting if we do not
		 * currently meet our desired number of search node parents.  The
		 * logic for determining our quota is actually in the child response
		 * for now, hopefully this will be changed to optimize away
		 * unnecessary network bandwidth.
		 */
		if (!(orig & FT_NODE_PARENT) && ft_conn_need_parents())
		{
			ft_session_add_purpose (node, FT_PURPOSE_PARENT_TRY);
			ft_packet_sendva (FT_CONN(node), FT_CHILD_REQUEST, 0, NULL);
		}

		/* make sure we cling on to all search peers when we are in a state
		 * of need */
		if (ft_conn_need_peers())
			ft_session_add_purpose (node, FT_PURPOSE_PEER_KEEP);
	}
}

static void log_class_change (FTNode *node, ft_class_t loss, ft_class_t gain,
                              ft_class_t klass)
{
	char   *hostfmt;
	String *changefmt;

	if (!(hostfmt = STRDUP (ft_node_fmt (node))))
		return;

	if (!(changefmt = string_new (NULL, 0, 0, TRUE)))
	{
		free (hostfmt);
		return;
	}

	string_appendc (changefmt, '(');

	if (gain)
	{
		string_appendf (changefmt, "+%s", ft_node_classstr (gain));

		/* add extra whitespace so that it doesn't look odd */
		if (loss)
			string_appendc (changefmt, ' ');
	}

	if (loss)
		string_appendf (changefmt, "-%s", ft_node_classstr (loss));

	string_appendc (changefmt, ')');

	/* actually write the log line */
	FT->dbg (FT, "%-24s %s %s", hostfmt, ft_node_classstr (klass),
	         changefmt->str);

	free (hostfmt);
	string_free (changefmt);
}

static void handle_class_change (FTNode *node,
								 ft_class_t orig,
								 ft_class_t loss,
                                 ft_class_t gain,
								 ft_class_t klass)
{
	/* ignore class changes that have occurred to our own internal node
	 * through the API */
	if (node->ninfo.host == 0)
		return;

	/* not connected */
	if (FT_CONN(node) == NULL)
		return;

	/* nothing happened */
	if (!loss && !gain)
		return;

	/* shutting down */
	if (openft->shutdown == TRUE)
		return;

	/* the API tells us we need to register class changes, although the
	 * implementation does not actually do anything with them */
	ft_netorg_change (node, orig, node->state);

	handle_class_loss (node, orig, loss);
	handle_class_gain (node, orig, gain);

	/* dump some tracking information to the user via the log file */
	log_class_change (node, loss, gain, klass);

#if 0
	/* some class change has occurred, invalidate the current html nodes
	 * list, so that it must be regenerated later */
	html_cache_flush ("nodes");
#endif
}

void ft_node_set_class (FTNode *node, ft_class_t klass)
{
	ft_class_t orig;
	ft_class_t loss, gain;

	assert (node != NULL);

	/* just in case */
	assert (FT_CONN(node) || !(klass & (FT_NODE_CHILD | FT_NODE_PARENT)));

	/* clamp the input value */
	klass &= FT_NODE_CLASS_MASK;
	klass |= FT_NODE_USER;

	/*
	 * Actually set the class here before handle_class_change because future
	 * logic is broken and will read node->klass directly to find out the new
	 * klass.  Save the original to determine gain and loss below.
	 */
	orig = node->ninfo.klass;
	node->ninfo.klass = klass;

	/*
	 * Determine what the class changes were.  This logic could probably fit
	 * just as well in handle_class_change, but it seems easier to break it
	 * up in here given that _add_class and _remove_class are not required
	 * to wrap this function.
	 */
	loss = orig & ~klass;
	gain = klass & ~orig;

	handle_class_change (node, orig, loss, gain, klass);
}

void ft_node_add_class (FTNode *node, ft_class_t klass)
{
	ft_node_set_class (node, node->ninfo.klass | klass);
}

void ft_node_remove_class (FTNode *node, ft_class_t klass)
{
	ft_node_set_class (node, node->ninfo.klass & ~klass);
}

ft_class_t ft_node_class (FTNode *node, BOOL session_info)
{
	ft_class_t klass;

	if (!node)
		return 0;

	klass = node->ninfo.klass;

	/* remove any session-specific class modifiers */
	if (!session_info)
		klass &= ~FT_NODE_CLASSMOD_MASK;

	return klass;
}

/*****************************************************************************/

char *ft_node_fmt (FTNode *node)
{
	char *hoststr;

	/* avert possible disaster */
	if (!node)
		return "(null)";

	hoststr = net_ip_str (node->ninfo.host);
	assert (hoststr != NULL);

	return stringf ("%s:%hu", hoststr, node->ninfo.port_openft);
}

char *ft_node_user (FTNode *node)
{
	if (!node)
		return NULL;

	return ft_node_user_host (node->ninfo.host, node->ninfo.alias);
}

char *ft_node_user_host (in_addr_t host, const char *alias)
{
	char *host_str;

	if (!(host_str = net_ip_str (host)))
		return NULL;

	if (is_valid_alias (alias) == TRUE)
		return stringf ("%s@%s", alias, host_str);

	return host_str;
}

BOOL ft_node_fw (FTNode *node)
{
	BOOL ret;

	assert (node != NULL);

	if ((ret = node->ninfo.indirect) == TRUE)
		assert (node->ninfo.port_openft == 0);
	else
	{
		assert (node->ninfo.port_openft > 0);

		if (node->session)
		{
			if (node->session->verified == FALSE)
				return TRUE;
		}
	}

	return ret;
}
