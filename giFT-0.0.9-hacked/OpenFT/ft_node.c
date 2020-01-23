/*
 * $Id: ft_node.c,v 1.29 2003/06/09 15:28:28 jasta Exp $
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

#include "ft_html.h"                   /* html_cache_flush */
#include "ft_netorg.h"
#include "ft_node.h"

/*****************************************************************************/

FTNode *ft_node_new (in_addr_t ip)
{
	FTNode *node;

	if (!(node = MALLOC (sizeof (FTNode))))
		return NULL;

	node->ip = ip;
	node->state = FT_NODE_DISCONNECTED;
	node->klass = FT_NODE_USER;

	return node;
}

static void flush_queue (Array **queue)
{
	FTPacket *packet;

	while ((packet = shift (queue)))
		ft_packet_free (packet);

	unset (queue);
}

void ft_node_free (FTNode *node)
{
	if (!node)
		return;

	/* this will destroy all the necessary associations for us */
	if (node->session)
	{
		ft_session_stop (FT_CONN(node));
		assert (node->session == NULL);
	}

	free (node->alias);
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
                               FTNodeClass klass,
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

void ft_node_err (FTNode *node, FTNodeError err, char *errtxt)
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
	 case FT_ERROR_SUCCESS:     family = "";              break;
	 case FT_ERROR_VERMISMATCH: family = "VerMismatch: "; break;
	 case FT_ERROR_UNKNOWN:     family = "";              break;
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

void ft_node_set_port (FTNode *node, in_port_t port)
{
	if (!node)
		return;

	node->port = port;

	/* search and index nodes are not allowed to be firewalled, remove the
	 * class */
	if (!port && (node->klass & (FT_NODE_SEARCH | FT_NODE_INDEX)))
	{
		ft_node_remove_class (node, FT_NODE_SEARCH);
		ft_node_remove_class (node, FT_NODE_INDEX);
	}
}

void ft_node_set_http_port (FTNode *node, in_port_t http_port)
{
	if (!node || !http_port)
		return;

	node->http_port = http_port;
}

char *ft_node_set_alias (FTNode *node, char *alias)
{
	size_t alias_len;

	if (!node)
		return NULL;

	alias_len = STRLEN (alias);

	/* destroy the previously installed node alias */
	free (node->alias);

	/*
	 * Apply alias restriction rules imposed by the protocol.  Please note
	 * that the protocol actually allows us to remove the offending
	 * characters and/or clamp the strings length if not appropriate.  We are
	 * merely voiding the entire alias if anything that violates the rules is
	 * found.
	 */
	if (alias_len == 0 || alias_len > 32 || strchr (alias, '@'))
		alias = NULL;

	node->alias = STRDUP (alias);

	return node->alias;
}

/*****************************************************************************/

void ft_node_queue (FTNode *node, FTPacket *packet)
{
	if (!node || !packet)
		return;

	assert (FT_CONN(node) == NULL);

	if (!push (&node->squeue, packet))
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
char *ft_node_classstr (FTNodeClass klass)
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

char *ft_node_classstr_full (FTNodeClass klass)
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
char *ft_node_statestr (FTNodeState state)
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

static void handle_state_change (FTNode *node, FTNodeState orig,
                                 FTNodeState now)
{
	/* notify the network organization code that a change has occurred and
	 * should be reflected in the appropriate data structures */
	ft_netorg_change (node, node->klass, orig);

	/* log this state change */
	FT->dbg (FT, "%s (%s) -> %s: %s",
	         ft_node_fmt (node),
	         ft_node_classstr (node->klass), ft_node_statestr (now),
	         ft_node_geterr (node));

	/* admittedly, we shoudl use this more than we do :( */
	html_cache_flush ("nodes");
}

void ft_node_set_state (FTNode *node, FTNodeState state)
{
	FTNodeState orig;

	if (!node)
		return;

	/* use FT_NODE_DISCONNECTED instead */
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

static int submit_to_index (FTNode *node, in_addr_t *ip)
{
	ft_packet_sendva (FT_CONN(node), FT_STATS_REQUEST, 0, "hI",
	                  3 /* REMOVE USERS SHARES */, *ip);
	return TRUE;
}

static void handle_class_loss (FTNode *node, FTNodeClass loss)
{
	/* gracefully unshare all files with the remote node if we detect that
	 * the parent modifier must be removed */
	if (loss & FT_NODE_PARENT)
		ft_packet_sendva (FT_CONN(node), FT_REMSHARE_REQUEST, 0, NULL);

	/* when a child is lost, all shares that were previously indexed by this
	 * user must be invalidated with the index node for stats purposes */
	if (loss & FT_NODE_CHILD)
	{
		ft_netorg_foreach (FT_NODE_INDEX, FT_NODE_CONNECTED, 0,
		                   FT_NETORG_FOREACH(submit_to_index), &node->ip);
	}
}

static void handle_class_gain (FTNode *node, FTNodeClass gain)
{
	/* if we have a new index node, ask for a current stats digest update
	 * from this node */
	if (gain & FT_NODE_INDEX)
	{
		ft_packet_sendva (FT_CONN(node), FT_STATS_REQUEST, 0, "h",
		                  1 /* RETRIEVE INFO */);
	}

	/*
	 * New search nodes should be contacted for parenting if we do not
	 * currently meet our desired number of search node parents.  The logic
	 * for determining our quota is actually in the child response for now,
	 * hopefully this will be changed to optimize away unnecessary network
	 * bandwidth.
	 */
	if (gain & FT_NODE_SEARCH && node->version >= OPENFT_0_0_9_6)
		ft_packet_sendva (FT_CONN(node), FT_CHILD_REQUEST, 0, NULL);
}

static void log_class_change (FTNode *node, FTNodeClass loss, FTNodeClass gain,
                              FTNodeClass klass)
{
	char   *hostfmt;
	String *changefmt;

	if (!(hostfmt = stringf_dup ("%s:%hu", net_ip_str (node->ip), node->port)))
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
								 FTNodeClass orig,
								 FTNodeClass loss,
                                 FTNodeClass gain,
								 FTNodeClass klass)
{
	/* ignore class changes that have occurred to our own internal node
	 * through the API */
	if (node->ip == 0)
		return;

	/* nothing happened */
	if (!loss && !gain)
		return;

	/* the API tells us we need to register class changes, although the
	 * implementation does not actually do anything with them */
	ft_netorg_change (node, orig, node->state);

	handle_class_loss (node, loss);
	handle_class_gain (node, gain);

	/* dump some tracking information to the user via the log file */
	log_class_change (node, loss, gain, klass);

	/* some class change has occurred, invalidate the current html nodes
	 * list, so that it must be regenerated later */
	html_cache_flush ("nodes");
}

void ft_node_set_class (FTNode *node, FTNodeClass klass)
{
	FTNodeClass orig;
	FTNodeClass loss, gain;

	/* disconnected nodes are not allowed to have class changes as they cannot
	 * easily be relied upon */
	if (!node || !FT_CONN(node))
		return;

	orig = node->klass;
	node->klass = klass;

	/*
	 * Determine what the class changes were.  This logic could probably fit
	 * just as well in handle_class_change, but it seems easier to break it
	 * up in here given that add and remove class end up calling this
	 * function.
	 */
	loss = orig & ~klass;
	gain = klass & ~orig;

	handle_class_change (node, orig, loss, gain, klass);
}

void ft_node_add_class (FTNode *node, FTNodeClass klass)
{
	ft_node_set_class (node, node->klass | klass);
}

void ft_node_remove_class (FTNode *node, FTNodeClass klass)
{
	ft_node_set_class (node, node->klass & ~klass);
}

FTNodeClass ft_node_class (FTNode *node, int session_info)
{
	FTNodeClass klass;

	if (!node)
		return 0;

	klass = node->klass;

	if (!session_info)
	{
		klass &= ~FT_NODE_PARENT;
		klass &= ~FT_NODE_CHILD;
	}

	return klass;
}

/*****************************************************************************/

char *ft_node_fmt (FTNode *node)
{
	/* keep crappy printf implementations happy */
	if (!node)
		return "(null)";

	return stringf ("%s:%hu", net_ip_str (node->ip), node->port);
}

char *ft_node_user (FTNode *node)
{
	if (!node)
		return NULL;

	return ft_node_user_host (node->ip, node->alias);
}

char *ft_node_user_host (in_addr_t host, char *alias)
{
	char *host_str;

	if (!(host_str = net_ip_str (host)))
		return NULL;

	if (STRING_NULL(alias))
		return stringf ("%s@%s", alias, host_str);

	return host_str;
}

int ft_node_fw (FTNode *node)
{
	assert (node != NULL);
	return (node->port == 0);
}
