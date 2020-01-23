/*
 * node.c
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

#include "openft.h"

#include "netorg.h"

#include "queue.h"
#include "nb.h"

#include "xfer.h"
#include "html.h"

#include "file.h"

#include "search.h"

/*****************************************************************************/

extern Connection *ft_self;
extern int         openft_shutdown;

/*****************************************************************************/

static time_t nodes_mtime;

/*****************************************************************************/

/* 6 minute heartbeat timeout */
#define MAX_HEARTBEAT 3

/*****************************************************************************/

/*
 * main node communication loop
 *
 * handle reading of packets from a node connection (at this point
 * indiscriminant as to whether it is a search or user node)
 */
void ft_node_connection (Protocol *p, Connection *c)
{
	NBRead *nb;
	unsigned short len;
	int n;

	/* grab a data buffer */
	nb = nb_active (c->fd);

	/* ok this nasty little hack here is executed so that we can maintain
	 * a single command buffer for len/command and the linear data
	 * stream to follow */
	len = nb->flag + FT_HEADER_LEN;

	if ((n = nb_read (nb, len, NULL)) <= 0)
	{
		node_disconnect (c);
		return;
	}

	/* we are connected if we got here ... */
	node_state_set (c, NODE_CONNECTED);
	NODE (c)->vitality = time (NULL);

	/* we successfully read a portion of the current packet, but not all of
	 * it ... come back when we have more data */
	if (!nb->term)
		return;

	/* packet length (nb->len is gauranteed > 2 here) */
	len = ntohs (*((ft_uint16 *)nb->data));

	if (nb->flag || len == 0) /* we read a complete packet */
	{
		FTPacket *packet;
		int       ret;

		/* reset */
		nb->flag = 0;

		/* this code is totally bastardized. */
		packet = ft_packet_new (ntohs (*((ft_uint16 *)(nb->data + 2))),
		                        nb->data + FT_HEADER_LEN, len);

		ret = protocol_handle_command (p, c, packet);

		ft_packet_free (packet);

		/* TODO - should we do this? */
		if (!ret)
		{
			TRACE_SOCK (("invalid packet, disconnecting"));

			/* bastards */
			NODE (c)->vitality = 0;
			NODE (c)->disconnect_cnt = 30;

			node_disconnect (c);
			return;
		}
	}
	else if (!nb->flag) /* we read a header block with non-zero length waiting
						 * on the socket */
	{
		nb->term = FALSE;
		nb->flag = len;
	}
}

/*****************************************************************************/

Connection *node_new (int fd)
{
	Connection        *c;
	Node              *node;
	struct sockaddr_in saddr;
	int                len;

	/* create parent holder */
	c = connection_new (openft_proto);

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

#if 0
static Connection *node_add (int fd)
{
	Connection *c;

	c = node_new (fd);
	conn_add (c);

	return c;
}
#endif

/* register a new node detected to determine if we should add it to the
 * connection list or whether or not we should connect to it */
Connection *node_register (unsigned long ip, signed short port,
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

	node_conn_set (c, ip, port, http_port);

	if (add)
		conn_add (c);

	/* override class if none are set */
	if (NODE (c)->class == NODE_NONE)
		node_class_set (c, klass);

	/* determine if the outgoing connection should be made */
	if (outgoing)
		ft_connect (c);

	return c;
}

/*****************************************************************************/

void node_free (Connection *c)
{
	free (c->data);
	connection_free (c);
}

void node_remove (Connection *c)
{
	conn_remove (c);

	node_disconnect (c);
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

static int finish_search (IFEventID id, Search *search, Connection *c)
{
	/* emulate a search reply
	 * NOTE: this will have no effect if called on a search this node didnt
	 * request
	 * NOTE2: BIG HACK! see search.c:search_reply! */
	search->ref = list_remove (search->ref, I_PTR (NODE (c)->ip));

	if (!search->ref)
		return TRUE;

	return FALSE;
}

/* this function _WILL_ be called anytime a qualified node disconnects...mmmm
 * abstraction */
void node_disconnect (Connection *c)
{
	int l_parent = FALSE;

	/* attempt to figure out if we're getting flooded w/ bogus requests */
	NODE (c)->disconnect_cnt++;

	/* make sure we remove all searches with this host pending...
	 * TODO -- there is a more efficient way of doing this if we attach
	 * it to the Node structure */
	search_foreach ((HashFunc) finish_search, c, TRUE);

	/* flush queue data */
	queue_flush (c);

	/* remove from select and close */
	input_remove (c);
	net_close (c->fd);

	/* make sure the call-back connections are dead */
	verify_clear (&(NODE (c)->port_verify));
	verify_clear (&(NODE (c)->http_port_verify));

	NODE (c)->verified = FALSE;

	/* ensure that this host has no shares to self */
	ft_share_remove_by_host (NODE (c)->ip, FALSE);

	/* get rid of all users digests (stats collection) and terminate any
	 * searches we may have add this node */
	if (NODE (c)->class & NODE_SEARCH)
	{
		ft_share_stats_remove (NODE (c)->ip, 0);

		/* TODO -- terminate search */
	}

	node_state_set (c, NODE_DISCONNECTED);
	NODE (c)->fd = c->fd = -1;

	/* flush the sent/recv list */
	NODE (c)->sent_list = FALSE;
	NODE (c)->recv_list = FALSE;

	/* reset heartbeat */
	NODE (c)->heartbeat = 0;

	/* temporary shares data */
	if (NODE (c)->shares_file)
	{
		fclose (NODE (c)->shares_file);
		NODE (c)->shares_file = NULL;
	}

	if (NODE (c)->shares_path)
	{
		unlink (NODE (c)->shares_path);
		free (NODE (c)->shares_path);
		NODE (c)->shares_path = NULL;
	}

	if (NODE (c)->cap)
	{
		dataset_clear_free (NODE (c)->cap);
		NODE (c)->cap = NULL;
	}

	memset (&(NODE (c)->stats), 0, sizeof (NODE (c)->stats));

	if (!openft_shutdown && NODE (c)->class & NODE_PARENT)
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

static int conn_sort_vit (Connection *a, Connection *b)
{
	if (NODE (a)->vitality > NODE (b)->vitality)
		return -1;
	else if (NODE (a)->vitality < NODE (b)->vitality)
		return 1;

	return 0;
}

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
		unsigned long  vitality;
		char          *ip;
		unsigned short port;
		unsigned short http_port;

		ptr = buf;

		/* [vitality] [ip]:[port] [http_port] */

		vitality  = ATOUL (string_sep (&ptr, " "));
		ip        =        string_sep (&ptr, ":");
		port      = ATOI  (string_sep (&ptr, " "));
		http_port = ATOI  (string_sep (&ptr, " "));

		if (!ip)
			continue;

		c = node_register (inet_addr (ip), port, http_port, NODE_NONE, FALSE);

		if (!c)
			continue;

		NODE (c)->vitality = vitality;
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

	fclose (f);
}

/*****************************************************************************/

static Connection *node_maintain_link (Connection *c, Node *node, FILE *f)
{
	fprintf (f, "%lu %s:%hu %hu\n", node->vitality, net_ip_str (node->ip),
			 node->port, node->http_port);

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
	ft_packet_send (c, FT_STATS_REQUEST, "%hu", 1 /* retrieve info */);

	return NULL;
}

static Connection *handle_heartbeat (Connection *c, Node *node, void *udata)
{
	/* they've reached their maximum */
	if (NODE (c)->heartbeat >= MAX_HEARTBEAT)
	{
		TRACE_SOCK (("heartbeat timeout"));
		node_disconnect (c);
		return NULL;
	}

	NODE (c)->heartbeat++;
	ft_packet_send (c, FT_PING_REQUEST, NULL);

	return NULL;
}

static Connection *dec_discon_cnt (Connection *c, Node *node, void *udata)
{
	if (NODE (c)->disconnect_cnt < 20)
		NODE (c)->disconnect_cnt = 0;
	else
		NODE (c)->disconnect_cnt -= 5;

	return NULL;
}

static Connection *locate_parents (Connection *c, Node *node, void *udata)
{
	if (NODE (c)->class & NODE_PARENT)
		return NULL;

	ft_packet_send (c, FT_CHILD_REQUEST, NULL);

	return NULL;
}

static Connection *upload_digest (Connection *c, Node *node, void *udata)
{
	int uploads;
	int max_uploads;

	uploads     = upload_length (NULL);
	max_uploads = upload_status ();

	ft_packet_send (c, FT_MODSHARE_REQUEST, "%hu%hu%hu",
	                3 /* SUBMIT MAX UPLOADS */, uploads, max_uploads);

	return NULL;
}

int node_maintain_links (void *udata)
{
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
	if (validate_share_submit ())
	{
		conn_foreach ((ConnForeachFunc) locate_parents, NULL,
		              NODE_SEARCH, NODE_CONNECTED, 1);
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
                                    unsigned long *ip)
{
	/* ip -> host order -> network order again ... sigh */
	ft_packet_send (c, FT_STATS_REQUEST, "%hu+I",
	                3 /* remove users shares */, *ip);

	return NULL;
}

static void handle_loss (Connection *c, unsigned short klass)
{
	/* unshare all files */
	if (klass & NODE_SEARCH)
		ft_packet_send (c, FT_MODSHARE_REQUEST, NULL);

	if (klass & NODE_CHILD)
	{
		conn_foreach ((ConnForeachFunc) submit_to_index, &(NODE (c)->ip),
		              NODE_INDEX, NODE_CONNECTED, 0);
	}
}

static void handle_gain (Connection *c, unsigned short klass)
{
	/* request the stats from this node */
	if (klass & NODE_INDEX)
		ft_packet_send (c, FT_STATS_REQUEST, "%hu", 1 /* retrieve info */);

	/* if the node has just changed to a search node try to become it's new
	 * child ... CHILD_RESPONSE may determine that we really don't need them
	 * but it certainly wont hurt to try here */
	if (klass & NODE_SEARCH)
		ft_packet_send (c, FT_CHILD_REQUEST, NULL);
}

static void log_change (Connection *c,
                        unsigned short gain, unsigned short loss)
{
	char fmt_host[24];
	char fmt_change[64];

	/* disconnected state changes are completely useless to see */
	if (NODE (c)->state != NODE_CONNECTED)
		return;

	snprintf (fmt_host, sizeof (fmt_host), "%s:%hu",
	          net_ip_str (NODE (c)->ip), NODE (c)->port);

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
	        node_class_str (NODE (c)->class),
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
	if (c == ft_self)
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
	if (NODE (c)->state == NODE_DISCONNECTED)
		return;

	handle_gain (c, gain);

	log_change (c, gain, loss);

	/* a change occurred, reflect this on the nodepage (kind of) */
	if (NODE (c)->state == NODE_CONNECTED)
		html_update_nodepage ();
}

/*****************************************************************************/

void node_class_set (Connection *c, unsigned short klass)
{
	unsigned short orig;

	orig = NODE (c)->class;
	NODE (c)->class = klass;

	/* we do not know if we lost or gained classes, handle that with this
	 * weird logic ;) */
	node_class_change (c, FALSE, (klass & ~orig), (orig & ~klass));
}

/*****************************************************************************/

void node_class_add (Connection *c, unsigned short klass)
{
	unsigned short orig;

	orig = NODE (c)->class;
	NODE (c)->class |= klass;

	/* we know that we only added a class, so we do not need a complete
	 * usage of this function */
	node_class_change (c, FALSE, (NODE (c)->class & ~orig), FALSE);
}

/*****************************************************************************/

void node_class_remove (Connection *c, unsigned short klass)
{
	unsigned short orig;

	orig = NODE (c)->class;
	NODE (c)->class &= ~klass;

	/* pass the changes along */
	node_class_change (c, FALSE, FALSE, (orig & ~(NODE (c)->class)));
}

/*****************************************************************************/

void node_state_set (Connection *c, unsigned short state)
{
	char           *state_str;
	unsigned short  orig;
	int             change = FALSE;

	orig = NODE (c)->state;

	if (NODE (c)->state != state)
	{
		NODE (c)->state = state;
		change = TRUE;
	}

	if (change)
		conn_change (c, orig);

	/* dont print useless info */
	if (!change || !NODE (c)->class || NODE (c)->vitality + 300 < time (NULL))
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
	if (NODE (c)->class != NODE_USER)
	{
		TRACE (("%s:%hu (%s) -> %s",
		        net_ip_str (NODE (c)->ip), NODE (c)->port,
		        node_class_str (NODE (c)->class), state_str));
	}

	if (state == NODE_CONNECTED)
	{
		/* if we just got connected, emulate a class change */
		node_class_change (c, TRUE, NODE (c)->class, FALSE);

		html_update_nodepage ();
	}
}

/*****************************************************************************/

void node_conn_set (Connection *c, unsigned long ip, signed long port,
                    signed long http_port)
{
	if (ip)
		NODE (c)->ip = ip;

	if (port >= 0)
	{
		NODE (c)->firewalled = (port == 0);
		NODE (c)->port = port;
	}

	if (http_port > 0)
		NODE (c)->http_port = http_port;

	html_update_nodepage ();

	/* lying bastards. */
	if (NODE (c)->firewalled && NODE (c)->class & (NODE_SEARCH | NODE_INDEX))
	{
		node_class_remove (c, NODE_SEARCH);
		node_class_remove (c, NODE_INDEX);
	}
}
