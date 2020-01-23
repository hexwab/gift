/*
 * ft_node.c
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

#include <stdarg.h> /* va lists */
#include <time.h>   /* time ()  */

#include "ft_openft.h"

#include "ft_netorg.h"

#include "queue.h"
#include "nb.h"

#include "ft_xfer.h"
#include "ft_html.h"

#include "file.h"

#include "ft_search.h"

/*****************************************************************************/

static time_t nodes_mtime = 0;

/*****************************************************************************/

/* 4 minute heartbeat timeout */
#define MAX_HEARTBEAT 2

/*****************************************************************************/

Connection *node_new (int fd)
{
	Connection        *c;
	Node              *node;
	struct sockaddr_in saddr;
	int                len;

	/* create parent holder */
	if (!(c = connection_new (openft_p)))
		return NULL;

	/* openft specific data */
	node = malloc (sizeof (Node));
	memset (node, 0, sizeof (Node));

	node->fd = fd;

	/* fill peer info */
	if (fd >= 0 && !node->ip)
	{
		len = sizeof (saddr);
		if (getpeername (fd, (struct sockaddr *) &saddr, &len) == 0)
			node->ip = saddr.sin_addr.s_addr;
	}

	node->incoming = FALSE;
	node->verified = FALSE;

	c->data = node;
	c->fd   = fd;

	return c;
}

/* register a new node detected to determine if we should add it to the
 * connection list or whether or not we should connect to it */
Connection *node_register (in_addr_t ip, signed short port,
                           signed short http_port, unsigned short klass,
                           int outgoing)
{
	Connection *c;
	int add = FALSE;

	if (!ip)
		return NULL;

	/* determine if this data is already in the connection list
	 * TODO - port must match as well */
	c = conn_lookup (ip);

	if (!c)
	{
		/* TODO - this api is crap */
		c = node_new (-1);
		add = TRUE;
	}

	node_conn_set (c, ip, port, http_port, NULL);

	if (add)
		conn_add (c);

	/* override class if none are set */
	if (FT_NODE(c)->class == NODE_NONE)
		node_class_set (c, klass);

	/* determine if the outgoing connection should be made */
	if (outgoing)
		ft_session_connect (c);

	return c;
}

/*****************************************************************************/

/* wth? */
void node_free (Connection *c)
{
	Node *node;

	if ((node = FT_NODE(c)))
	{
		free (node->alias);
		free (node);
	}

	connection_free (c);
}

void node_remove (Connection *c)
{
	conn_remove (c);

	ft_session_stop (c);
	node_free (c);
}

/*****************************************************************************/

static void verify_clear (Connection **verify)
{
	if (*verify)
	{
		connection_close (*verify);
		*verify = NULL;
	}
}

static int free_stream (Dataset *d, DatasetNode *node, void *udata)
{
	ft_stream_free (node->value);
	return TRUE;
}

static void clear_stream (Dataset **d)
{
	dataset_foreach (*d, DATASET_FOREACH (free_stream), NULL);
	dataset_clear (*d);
	*d = NULL;
}

/* this function _WILL_ be called anytime a qualified node disconnects...mmmm
 * abstraction */
void node_disconnect (Connection *c)
{
	int l_parent = FALSE;

	/* attempt to figure out if we're getting flooded w/ bogus requests */
	FT_NODE(c)->disconnect_cnt++;

	/* make sure we remove all searches with this host pending */
	ft_search_force_reply (NULL, FT_NODE(c)->ip);

	/* flush queue data */
	queue_flush (c);

	/* remove from select and close */
	input_remove (c);
	net_close (c->fd);

	clear_stream (&(FT_NODE(c)->streams_recv));
	clear_stream (&(FT_NODE(c)->streams_send));

	/* make sure the call-back connections are dead */
	verify_clear (&(FT_NODE(c)->port_verify));
	verify_clear (&(FT_NODE(c)->http_port_verify));

	FT_NODE(c)->verified = FALSE;

	/* ensure that this host has no shares to self */
	ft_shost_remove (FT_NODE(c)->ip);

	/* get rid of all users digests (stats collection) and terminate any
	 * searches we may have add this node */
	if (FT_NODE(c)->class & NODE_SEARCH)
		ft_stats_remove (FT_NODE(c)->ip, 0);

	node_state_set (c, NODE_DISCONNECTED);
	FT_NODE(c)->fd = c->fd = -1;

	/* flush the sent/recv list */
	FT_NODE(c)->sent_list = FALSE;
	FT_NODE(c)->recv_list = FALSE;

	/* reset heartbeat */
	FT_NODE(c)->heartbeat = 0;

	/* temporary shares data */
	if (FT_NODE(c)->shares_file)
	{
		fclose (FT_NODE(c)->shares_file);
		FT_NODE(c)->shares_file = NULL;
	}

	if (FT_NODE(c)->shares_path)
	{
		unlink (FT_NODE(c)->shares_path);
		free (FT_NODE(c)->shares_path);
		FT_NODE(c)->shares_path = NULL;
	}

	if (FT_NODE(c)->cap)
	{
		dataset_clear (FT_NODE(c)->cap);
		FT_NODE(c)->cap = NULL;
	}

	if (FT_NODE(c)->alias)
	{
		free (FT_NODE(c)->alias);
		FT_NODE(c)->alias = NULL;
	}

	memset (&(FT_NODE(c)->stats), 0, sizeof (FT_NODE(c)->stats));

	if (!OPENFT->shutdown && FT_NODE(c)->class & NODE_PARENT)
		l_parent = TRUE;

	/* remove all temporary conditions */
	node_class_remove (c, NODE_CHILD);
	node_class_remove (c, NODE_PARENT);

	/* locate a new parent :) */
	if (l_parent && validate_share_submit ())
		ft_share_local_submit (NULL);
}

/*****************************************************************************/

unsigned short node_highest_class (Node *node)
{
	unsigned short high_class;

	if (node->class & NODE_INDEX)
		high_class = NODE_INDEX;
	else if (node->class & NODE_SEARCH)
		high_class = NODE_SEARCH;
	else if (node->class & NODE_USER)
		high_class = NODE_USER;
	else
		high_class = NODE_NONE;

	return high_class;
}

/*****************************************************************************/

/* return a < b */
static int conn_sort_vit (Connection *a, Connection *b)
{
	int ret;

	/* sort by version, last_session, then uptime */
	if ((ret = INTCMP(FT_NODE(a)->version, FT_NODE(b)->version)))
		return -ret;

	if ((ret = INTCMP(FT_NODE(a)->last_session, FT_NODE(b)->last_session)))
		return -ret;

	if ((ret = INTCMP(FT_NODE(a)->uptime, FT_NODE(b)->uptime)))
		return -ret;

	return 0;
}

#if 0
static Connection *conn_dump (Connection *c, Node *node, void *udata)
{
	TRACE (("%s %li %li", net_ip_str (node->ip),
	        (long)node->last_session, (long)node->uptime));
	return NULL;
}
#endif

/* load up the initial nodes file */
static void node_load_cache ()
{
	Connection *c;
	FILE *f;
	char *nodes;
	char *buf = NULL;
	char *ptr;

	nodes = gift_conf_path ("OpenFT/nodes");

	TRACE (("[re]loading node cache: %s...", nodes));

	/* start the initial connections */
	if (!(f = fopen (nodes, "r")))
	{
		char filename[PATH_MAX];

		/* try the global nodes file */
		snprintf (filename, sizeof (filename) - 1,
		          "%s/OpenFT/nodes", platform_data_dir ());

		f = fopen (filename, "r");
	}

	if (!f)
	{
		GIFT_WARN (("unable to locate a nodes file...sigh"));
		return;
	}

	while (file_read_line (f, &buf))
	{
		time_t         vitality;
		time_t         uptime;
		char          *host;
		unsigned short port;
		unsigned short http_port;
		unsigned short klass;
		ft_uint32      version;

		ptr = buf;

		/* [last_packet] [total_uptime] [host] [ftport] [httpport] [class]
		 *     [version] */

		vitality  = (time_t)        ATOUL(string_sep (&ptr, " "));
		uptime    = (time_t)        ATOUL(string_sep (&ptr, " "));
		host      =                       string_sep (&ptr, " ");
		port      = (unsigned short) ATOI(string_sep (&ptr, " "));
		http_port = (unsigned short) ATOI(string_sep (&ptr, " "));
		klass     = (unsigned short) ATOI(string_sep (&ptr, " "));
		version   = (ft_uint32)     ATOUL(string_sep (&ptr, " "));

		if (!host || !version)
			continue;

		if (!(c = node_register (net_ip (host), port, http_port, klass, FALSE)))
			continue;

		FT_NODE(c)->last_session = vitality;
		FT_NODE(c)->uptime       = uptime;
		FT_NODE(c)->version      = version;
	}

	if (!conn_length (NODE_NONE, -1))
	{
		GIFT_WARN (("No nodes loaded.  If you believe this is in error, try "
		            "removing your local nodes file at %s.  This will cause "
		            "this module to repopulate its list from the global "
		            "cache.", nodes));
	}

	/* hit the most vital first */
	conn_sort ((CompareFunc) conn_sort_vit);

#if 0
	conn_foreach ((ConnForeachFunc) conn_dump, NULL, NODE_NONE, -1, 0);
#endif

	fclose (f);
}

/*****************************************************************************/

static Connection *node_maintain_link (Connection *c, Node *node, FILE *f)
{
	time_t         vitality;
	time_t         uptime;
	unsigned short klass;

	if (node->state == NODE_CONNECTED)
		vitality = time (NULL);
	else
		vitality = FT_NODE(c)->last_session;

	uptime = FT_NODE(c)->uptime + ft_session_uptime (c);

	klass = node->class & ~(NODE_CHILD | NODE_PARENT);

	fprintf (f, "%li %li %s %hu %hu %hu %u\n",
			 (long)vitality, (long)uptime,
			 net_ip_str (node->ip), node->port, node->http_port,
			 (unsigned short)klass, (unsigned int)node->version);

	return NULL;
}

void node_update_cache ()
{
	FILE  *f;
	char  *nodes;
	time_t mtime = 0;

	nodes = gift_conf_path ("OpenFT/nodes");

	/*
	 * check to see if we need to reload the nodes file.  this occurs when
	 * the mtime on disk doesnt match the last time we wrote to the file or
	 * the file has been deleted since the last check, in which case
	 * node_load_cache will look for the globally installed nodes file
	 */
	if ((!(file_exists (nodes, NULL, &mtime)) && nodes_mtime) ||
	    nodes_mtime != mtime)
	{
		/* the user has changed the nodes file on disk, reload it */
		node_load_cache ();
	}

	if (!file_exists (nodes, NULL, NULL))
	{
		/* basically show users that SIGINT rewrites this file :P */
		GIFT_WARN (("*** creating new nodes file: %s", nodes));
	}

	if (!(f = fopen (nodes, "w")))
	{
		GIFT_ERROR(("Can't create %s: %s", nodes, GIFT_STRERROR () ));
		return;
	}

	conn_foreach ((ConnForeachFunc) node_maintain_link, f,
	              NODE_NONE, -1, 0);

	fclose (f);

	if ((file_exists (nodes, NULL, &mtime)))
		nodes_mtime = mtime;
}

/*****************************************************************************/

static Connection *node_stats (Connection *c, Node *node, void *udata)
{
	ft_packet_sendva (c, FT_STATS_REQUEST, 0, "h", 1 /* retrieve info */);
	return NULL;
}

static Connection *handle_heartbeat (Connection *c, Node *node, void *udata)
{
	/* they've reached their maximum */
	if (FT_NODE(c)->heartbeat >= MAX_HEARTBEAT)
	{
		TRACE_SOCK (("heartbeat timeout"));
		ft_session_stop (c);
		return NULL;
	}

	FT_NODE(c)->heartbeat++;
	ft_packet_sendva (c, FT_PING_REQUEST, 0, NULL);

	return NULL;
}

static Connection *dec_discon_cnt (Connection *c, Node *node, void *udata)
{
	if (FT_NODE(c)->disconnect_cnt < 2)
		FT_NODE(c)->disconnect_cnt = 0;
	else
		FT_NODE(c)->disconnect_cnt -= 2;

	return NULL;
}

static Connection *locate_parents (Connection *c, Node *node, void *udata)
{
	if (FT_NODE(c)->class & NODE_PARENT)
		return NULL;

	ft_packet_sendva (c, FT_CHILD_REQUEST, 0, NULL);

	return NULL;
}

static Connection *upload_digest (Connection *c, Node *node, void *udata)
{
	ft_packet_sendva (c, FT_MODSHARE_REQUEST, 0, "hl", FALSE,
	                  upload_availability ());
	return NULL;
}

int node_maintain_links (void *udata)
{
	int needed;

	/* the nodes file was never written, therefore never read, so load it up
	 * here to make sure it gets loaded appropriately */
	if (!nodes_mtime)
		node_load_cache ();

	/* disconnect_cnt's should be flushed as to re-test their ability to
	 * behave properly :) */
	conn_foreach ((ConnForeachFunc) dec_discon_cnt, NULL,
	              NODE_NONE, -1, 0);

	/* send out the OpenFT heartbeat...each time a heartbeat is sent out w/o
	 * a response it will increase a counter that eventually will lead to this
	 * nodes disconnection */
	conn_foreach ((ConnForeachFunc) handle_heartbeat, NULL,
	              NODE_USER, NODE_CONNECTED, 0);

	/* make sure our parent count is satisfied */
	if ((needed = parents_needed ()))
	{
		conn_foreach ((ConnForeachFunc) locate_parents, NULL,
		              NODE_SEARCH, NODE_CONNECTED, needed);
	}

	/* just for good measure make sure we send our upload report every
	 * 2 minutes to catch problems */
	conn_foreach ((ConnForeachFunc) upload_digest, NULL,
				  NODE_PARENT, NODE_CONNECTED, 0);

	/* gather stats information */
	conn_foreach ((ConnForeachFunc) node_stats, NULL,
	              NODE_INDEX, NODE_CONNECTED, 0);

	/* maintain network organization */
	conn_maintain ();

	node_update_cache ();

	return TRUE;
}

/*****************************************************************************/

/* notify the index node that this users shares have been removed */
static Connection *submit_to_index (Connection *c, Node *node,
                                    in_addr_t *ip)
{
	/* ip -> host order -> network order again ... sigh */
	ft_packet_sendva (c, FT_STATS_REQUEST, 0, "hI",
					  3 /* remove users shares */, *ip);

	return NULL;
}

static void handle_loss (Connection *c, unsigned short klass)
{
	/* unshare all files */
	if (klass & NODE_SEARCH)
		ft_packet_sendva (c, FT_REMSHARE_REQUEST, 0, NULL);

	if (klass & NODE_CHILD)
	{
		conn_foreach ((ConnForeachFunc) submit_to_index, &(FT_NODE(c)->ip),
		              NODE_INDEX, NODE_CONNECTED, 0);
	}
}

static void handle_gain (Connection *c, unsigned short klass)
{
	/* request the stats from this node */
	if (klass & NODE_INDEX)
		ft_packet_sendva (c, FT_STATS_REQUEST, 0, "h", 1 /* retrieve info */);

	/* if the node has just changed to a search node try to become it's new
	 * child ... CHILD_RESPONSE may determine that we really don't need them
	 * but it certainly wont hurt to try here */
	if (klass & NODE_SEARCH)
		ft_packet_sendva (c, FT_CHILD_REQUEST, 0, NULL);
}

static void log_change (Connection *c,
                        unsigned short gain, unsigned short loss)
{
	char fmt_host[24];
	char fmt_change[64];

	/* disconnected state changes are completely useless to see */
	if (FT_NODE(c)->state != NODE_CONNECTED)
		return;

	snprintf (fmt_host, sizeof (fmt_host), "%s:%hu",
	          net_ip_str (FT_NODE(c)->ip), FT_NODE(c)->port);

	if (!gain && !loss)
		*fmt_change = 0;
	else
	{
		int len = 0;

		len = sprintf (fmt_change, "(");

		if (gain)
		{
			len += snprintf (fmt_change + len, sizeof (fmt_change) - len - 1,
			                 "+%s%s", node_class_str (gain), (loss ? " " : ""));
		}

		if (loss)
		{
			len += snprintf (fmt_change + len, sizeof (fmt_change) - len - 1,
			                 "-%s", node_class_str (loss));
		}

		snprintf (fmt_change + len, sizeof (fmt_change) - len - 1, ")");
	}

	TRACE (("%-24s %s %s",
	        fmt_host,
	        node_class_str (FT_NODE(c)->class),
			fmt_change));
}

/* this function handles __EVERY__ class change that occurs on the
 * network */
static void node_class_change (Connection *c,
							   int emulate,
                               unsigned short gain,
                               unsigned short loss)
{
	/* ignore local class changes...expect that they are handled on an
	 * individual level */
	if (c == OPENFT->ft)
		return;

	/* nothing happened */
	if (!gain && !loss)
	{
		/* log emulations */
		if (emulate)
			log_change (c, gain, loss);

		return;
	}

	handle_loss (c, loss);

	/* the class change will be emulated when its connected so we can deal
	 * w/ it */
	if (FT_NODE(c)->state == NODE_DISCONNECTED)
		return;

	handle_gain (c, gain);

	log_change (c, gain, loss);

	/* a change occurred, reflect this on the nodepage (kind of) */
	if (FT_NODE(c)->state == NODE_CONNECTED)
		html_cache_flush ("nodes");
}

/*****************************************************************************/

void node_class_set (Connection *c, unsigned short klass)
{
	unsigned short orig;

	orig = FT_NODE(c)->class;
	FT_NODE(c)->class = klass;

	/* we do not know if we lost or gained classes, handle that with this
	 * weird logic ;) */
	node_class_change (c, FALSE, (klass & ~orig), (orig & ~klass));
}

/*****************************************************************************/

void node_class_add (Connection *c, unsigned short klass)
{
	unsigned short orig;

	orig = FT_NODE(c)->class;
	FT_NODE(c)->class |= klass;

	/* we know that we only added a class, so we do not need a complete
	 * usage of this function */
	node_class_change (c, FALSE, (FT_NODE(c)->class & ~orig), FALSE);
}

/*****************************************************************************/

void node_class_remove (Connection *c, unsigned short klass)
{
	unsigned short orig;

	orig = FT_NODE(c)->class;
	FT_NODE(c)->class &= ~klass;

	/* pass the changes along */
	node_class_change (c, FALSE, FALSE, (orig & ~(FT_NODE(c)->class)));
}

/*****************************************************************************/

void node_state_set (Connection *c, unsigned short state)
{
	char           *state_str;
	unsigned short  orig;
	int             change = FALSE;

	orig = FT_NODE(c)->state;

	if (FT_NODE(c)->state != state)
	{
		FT_NODE(c)->state = state;
		change = TRUE;
	}

	if (change)
		conn_change (c, orig);

	/* dont print useless info */
	if (!change || !FT_NODE(c)->class || FT_NODE(c)->last_session + 300 < time (NULL))
		return;

	switch (state)
	{
	 case NODE_CONNECTED:
		state_str = "FINAL";
		break;
	 case NODE_CONNECTING:
		state_str = "LIMBO";
		break;
	 default:
		state_str = "DISCO";
		break;
	}

	/* to be honest NODE_USER connections are just used to gather more
	 * nodes in your connection list, hardly worth notifying the user :) */
	if (FT_NODE(c)->class != NODE_USER)
	{
		TRACE (("%s:%hu (%s) -> %s",
		        net_ip_str (FT_NODE(c)->ip), FT_NODE(c)->port,
		        node_class_str (FT_NODE(c)->class), state_str));
	}

	if (state == NODE_CONNECTED)
	{
		/* if we just got connected, emulate a class change */
		node_class_change (c, TRUE, FT_NODE(c)->class, FALSE);
		html_cache_flush ("nodes");
	}
}

/*****************************************************************************/

void node_conn_set (Connection *c, in_addr_t ip, signed long port,
                    signed long http_port, char *alias)
{
	if (ip)
		FT_NODE(c)->ip = ip;

	if (port >= 0)
	{
		FT_NODE(c)->firewalled = (port == 0);
		FT_NODE(c)->port = port;
	}

	if (http_port > 0)
		FT_NODE(c)->http_port = http_port;

	if (alias)
	{
		size_t alias_len = strlen (alias);

		free (FT_NODE(c)->alias);

		/* nothing crazy should go here */
		if (alias_len == 0 || alias_len > 64 || strchr (alias, '@'))
			alias = NULL;

		FT_NODE(c)->alias = STRDUP (alias);
	}

	html_cache_flush ("nodes");

	/* lying bastards. */
	if (FT_NODE(c)->firewalled && FT_NODE(c)->class & (NODE_SEARCH | NODE_INDEX))
	{
		node_class_remove (c, NODE_SEARCH);
		node_class_remove (c, NODE_INDEX);
	}
}

/*****************************************************************************/

char *node_user (in_addr_t host, char *alias)
{
	char *host_str;

	if (!(host_str = net_ip_str (host)))
		return NULL;

	if (STRING_NULL(alias))
		return stringf ("%s@%s", alias, host_str);

	return host_str;
}
