/*
 * $Id: gt_bind.c,v 1.6 2004/04/17 06:08:14 hipnod Exp $
 *
 * Copyright (C) 2001-2004 giFT project (gift.sourceforge.net)
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

#include "gt_gnutella.h"
#include "gt_node.h"
#include "gt_node_list.h"

#include "gt_accept.h"
#include "gt_bind.h"
#include "gt_packet.h"
#include "gt_share_state.h"

#include "transfer/push_proxy.h"

/*****************************************************************************/

/* global node pointer for this machine */
GtNode *GT_SELF;

/*****************************************************************************/

/* how often to retest the firewalled status of this node */
#define FW_RETEST_INTERVAL                (60 * MINUTES)

/* maximum amount of time before last connect read from disk before
 * reassuming this node is firewalled on startup */
#define FW_MAX_RETEST_TIME                (7 * EDAYS)

/*****************************************************************************/

/* time at which this node started running */
static time_t     start_time;

/* last time we got a connection on this host */
static time_t     last_connect;

/* retest firewalled status every so often */
static timer_id   fw_test_timer;

/*****************************************************************************/

static char *fw_file (void)
{
	return gift_conf_path ("Gnutella/fwstatus");
}

static void save_fw_status (void)
{
	FILE  *f;

	if (!(f = fopen (fw_file (), "w")))
		return;

	/*
	 * Store the last successful time of connect along with the port.
	 */
	fprintf (f, "%lu %hu\n", last_connect, GT_SELF->gt_port);

	fclose (f);
}

/* returns whether or not the node is firewalled */
static BOOL load_fw_status (in_port_t port)
{
	FILE      *f;
	char       buf[RW_BUFFER];
	time_t     last_time;
	in_port_t  last_port;

	if (!(f = fopen (fw_file (), "r")))
		return TRUE;

	if (fgets (buf, sizeof (buf) - 1, f) == NULL)
	{
		fclose (f);
		return TRUE;
	}

	fclose (f);

	/* Store the time of the last successful connect, so
	 * > 0 means _not_ firewalled */
	if (sscanf (buf, "%lu %hu", &last_time, &last_port) != 2)
		return TRUE;

	/* if we got a connection within a recent time at some point with this
	 * port, mark not firewalled */
	if ((last_time > 0 && last_time < FW_MAX_RETEST_TIME) &&
	    last_port == port)
	{
		last_connect = last_time;
		return FALSE;
	}

	return TRUE;
}

static gt_node_class_t read_class (void)
{
	char *klass;

	klass = gt_config_get_str ("main/class");

	if (klass && strstr (klass, "ultra"))
		return GT_NODE_ULTRA;

	return GT_NODE_LEAF;
}

static void setup_self (GtNode *node, TCPC *c, in_port_t port)
{
	/* load the firewalled status of this port */
	node->firewalled = load_fw_status (port);

	/* attach the connection to this node */
	gt_node_connect (node, c);
	node->gt_port = port;

	/* load the class for this node */
	node->klass = read_class ();

	input_add (c->fd, c, INPUT_READ,
	           (InputCallback)gnutella_handle_incoming, FALSE);
}

static GtNode *bind_gnutella_port (in_port_t port)
{
	GtNode  *node;
	TCPC    *c;

	GT->DBGFN (GT, "entered");

	if (!(node = gt_node_new ()))
		return NULL;

	/* assume sane defaults in case the bind fails */
	node->gt_port    = 0;
	node->firewalled = TRUE;
	node->klass      = GT_NODE_LEAF;

	if (!port || !(c = tcp_bind (port, FALSE)))
	{
		GT->warn (GT, "Failed binding port %d, setting firewalled", port);
		return node;
	}

	GT->dbg (GT, "bound to port %d", port);

	/* setup what will become GT_SELF structure */
	setup_self (node, c, port);

	return node;
}

static void setup_listening_port (in_port_t port)
{
	GT_SELF = bind_gnutella_port (port);

	/*
	 * If running in local mode, let the user set firewalled status in
	 * GNUTELLA_LOCAL_FW.  Not sure if this is a good idea, but it makes
	 * testing easier.
	 */
	if (GNUTELLA_LOCAL_MODE)
	{
		if (GNUTELLA_LOCAL_FW)
			GT_SELF->firewalled = TRUE;
		else
			GT_SELF->firewalled = FALSE;
	}
}

/*****************************************************************************/

BOOL gt_bind_is_firewalled (void)
{
	if (!GT_SELF->firewalled)
		return FALSE;

	/*
	 * Pretend we are not firewalled at the beginning in order
	 * to possibly get more connections, to prove we are not firewalled.
	 */
	if (gt_uptime () < 10 * EMINUTES)
		return FALSE;

	/* we are firewalled */
	return TRUE;
}

void gt_bind_clear_firewalled (void)
{
	time (&last_connect);
	GT_SELF->firewalled = FALSE;
}

static BOOL fwtest_node (GtNode *node)
{
	GtPacket *pkt;
	BOOL      ret;

	if (!GT_SELF->firewalled)
		return FALSE;

	if (!(pkt = gt_packet_vendor (GT_VMSG_TCP_CONNECT_BACK)))
		return FALSE;

	gt_packet_put_port (pkt, GT_SELF->gt_port);
	GT->DBGSOCK (GT, GT_CONN(node), "fwtesting");

	ret = gt_node_send_if_supported (node, pkt);
	gt_packet_free (pkt);

	return ret;
}

/*****************************************************************************/

static void push_proxy_request (GtNode *node)
{
	GtPacket *pkt;

	if (!(pkt = gt_packet_vendor (GT_VMSG_PUSH_PROXY_REQ)))
		return;

	/* the GUID of the PushProxyRequest must be our client identifier */
	gt_packet_set_guid (pkt, GT_SELF_GUID);

	gt_node_send_if_supported (node, pkt);

	gt_packet_free (pkt);
}

/*****************************************************************************/

/*
 * Called when a new connection to a node has completed.
 */
void gt_bind_completed_connection (GtNode *node)
{
	if (node->vmsgs_sent && dataset_length (node->vmsgs_supported) > 0)
		return;

	node->vmsgs_sent = TRUE;

	fwtest_node (node);
	push_proxy_request (node);
}

/*****************************************************************************/

static GtNode *retest (TCPC *c, GtNode *node, void *udata)
{
	/*
	 * Only clear firewalled status once and if the node supports
	 * the TcpConnectBack message.
	 */
	if (fwtest_node (node) && GT_SELF->firewalled == FALSE)
	{
		GT->DBGFN (GT, "clearing firewalled status");
		GT_SELF->firewalled = TRUE;
	}

	return NULL;
}

static BOOL fw_test (void *udata)
{
	gt_conn_foreach (retest, NULL,
	                 GT_NODE_NONE, GT_NODE_CONNECTED, 0);

	return TRUE;
}

/*****************************************************************************/

time_t gt_uptime (void)
{
	return start_time;
}

/*****************************************************************************/

void gt_bind_init (void)
{
	int port;

	port = gt_config_get_int ("main/port=6346");
	setup_listening_port (port);

	time (&start_time);

	fw_test_timer = timer_add  (FW_RETEST_INTERVAL, fw_test, NULL);
}

void gt_bind_cleanup (void)
{
	save_fw_status ();

	/* gt_node_free() will remove the listening input callback */
	gt_node_free (GT_SELF);
	GT_SELF = NULL;

	start_time   = 0;
	last_connect = 0;

	timer_remove_zero (&fw_test_timer);
}
