/*
 * utils.c
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

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>

#include "openft.h"

#include "netorg.h"

#include "xfer.h"

#include "log.h"

/*****************************************************************************/

extern Config     *openft_conf;
extern Connection *ft_self;

/*****************************************************************************/

/* maximum parents to submit to */
#define MAX_PARENTS config_get_int (openft_conf, "sharing/parents=3")

/*****************************************************************************/

ft_uint32 ft_make_version (unsigned short major, unsigned short minor,
                           unsigned short micro)
{
	ft_uint32 version;

	version = ((major & 0xff) << 16 | (minor & 0xff) << 8 | (micro & 0xff));

	return version;
}

/*****************************************************************************/

static void accept_test_result (Connection *c, Connection *verify, int success)
{
	/* what the hell? */
	if (!c)
		return;

	/* TODO - notify this connection that it's configured port has been
	 * changed */

	/* one of the verification sockets failed, those bastards */
	if (!success && NODE (c)->port != 0)
	{
		/* set ip as 0 (firewalled) */
		node_conn_set (c, 0, 0, -1);
	}

	if (verify)
	{
		if (verify == NODE (c)->port_verify)
			NODE (c)->port_verify = NULL;
		else
		if (verify == NODE (c)->http_port_verify)
			NODE (c)->http_port_verify = NULL;

		/* close up the socket ... sigh ... this really doesnt belong here */
		net_close (verify->fd);
		input_remove (verify);
		connection_free (verify);
	}

	/* do not set verified to TRUE until both call-back connections return */
	if (NODE (c)->port_verify || NODE (c)->http_port_verify)
		return;

	NODE (c)->verified = TRUE;

	/* set the host share element appropriately, so that searches may be
	 * effectively returned */
	ft_share_verified (NODE (c)->ip);
}

static void accept_test_verify (Protocol *p, Connection *verify)
{
	Connection *c;

	c = verify->data;

	/* they lied.  we can't connect to them */
	if (net_sock_error (verify->fd))
	{
		accept_test_result (c, verify, FALSE);
		return;
	}

	accept_test_result (c, verify, TRUE);
}

static void accept_test_port (Connection *c, unsigned short port,
                              Connection **verify)
{
	if (*verify)
	{
		input_remove (*verify);
		net_close ((*verify)->fd);
	}
	else
		*verify = connection_new (openft_proto);

	(*verify)->fd   = net_connect (net_ip_str (NODE (c)->ip), port, FALSE);
	(*verify)->data = c; /* set parent */

	/* TODO - if this happens self is likely broken, not the incoming
	 * connection. what to do, what to do */
	if ((*verify)->fd <= 0)
	{
		accept_test_result (c, *verify, FALSE);
		return;
	}

	/* let select handle the rest of the verification */
	input_add (openft_proto, *verify, INPUT_WRITE,
	           (InputCallback) accept_test_verify, TRUE);
}

/* connect to the given node in order to verify their port configuration is
 * accurate */
void ft_accept_test (Connection *c)
{
	if (NODE (c)->verified)
	{
		TRACE_SOCK (("connection already verified"));
		return;
	}

	if (!NODE (c)->ip || NODE (c)->port == 0)
	{
		accept_test_result (c, NULL, FALSE);
		return;
	}

	/* if either of these fail, the same result (accept_test_result) will
	 * occur */
	accept_test_port (c, NODE (c)->port,      &(NODE (c)->port_verify));
	accept_test_port (c, NODE (c)->http_port, &(NODE (c)->http_port_verify));

	return;
}

/*****************************************************************************/

/* connection verification */
static int ft_handle_verification (Protocol *p, Connection *c)
{
	/* make sure we got a valid connection */
	if (net_sock_error (c->fd))
	{
		node_disconnect (c);

		return FALSE;
	}

	/* switch to a new cb */
	input_remove (c);
	input_add (p, c, INPUT_READ, (InputCallback) ft_node_connection, TRUE);

	/* HANDSHAKE -- TODO - move this/clean up */
	ft_packet_send (c, FT_VERSION_REQUEST, NULL);

	/* always attempt to update our node information */
	ft_packet_send (c, FT_NODEINFO_REQUEST, NULL);

	/* leech as much info from this node as possible... */
	ft_packet_send (c, FT_CLASS_REQUEST, NULL);
	ft_packet_send (c, FT_NODELIST_REQUEST, NULL);

	/* exchange node capabilities */
	ft_packet_send (c, FT_NODECAP_REQUEST, NULL);

	return TRUE;
}

/* outgoing connections callback */
static void ft_handle_source (Protocol *p, Connection *c)
{
	if (!ft_handle_verification (p, c))
		return;
}

/*****************************************************************************/

int ft_connect (Connection *c)
{
	/* only connect disconnected nodes */
	if (c->fd >= 0)
		return -1;

	if (NODE (c)->state != NODE_DISCONNECTED)
	{
		GIFT_DEBUG (("state = %i??", NODE (c)->state));
		return -1;
	}

	if (!conn_auth (c, TRUE))
		return -1;

	/* make outgoing connection */
	c->fd        = net_connect (net_ip_str (NODE (c)->ip), NODE (c)->port, FALSE);
	NODE (c)->fd = c->fd;

	node_state_set (c, NODE_CONNECTING);

	/* wait for response */
	input_add (openft_proto, c, INPUT_WRITE,
			   (InputCallback) ft_handle_source, TRUE);

	return c->fd;
}

/* incoming connections callback */
void ft_handle_incoming (Protocol *p, Connection *c)
{
	Connection *new_c;
	int fd;

	fd = net_accept (c->fd, FALSE);

	if (fd < 0)
		return;

	new_c = node_register (inet_addr (net_peer_ip (fd)), -1, -1,
	                       NODE_NONE, FALSE);

	if (!new_c)
	{
		net_close (fd);
		return;
	}

	/* do not deal with an already connect{ed,ing} socket */
	if (NODE (new_c)->state != NODE_DISCONNECTED ||
		new_c->fd >= 0)
	{
		net_close (fd);
		return;
	}

	if (!conn_auth (new_c, FALSE))
	{
		net_close (fd);
		return;
	}

	new_c->fd        = fd;
	NODE (new_c)->fd = fd;

	NODE (new_c)->incoming = TRUE;
	node_state_set (new_c, NODE_CONNECTING);
	node_class_set (new_c, NODE_NONE);

	/* throw it into the select loop */
	input_add (p, new_c, INPUT_WRITE,
			   (InputCallback) ft_handle_verification, TRUE);
}

/*****************************************************************************/

int validate_share_submit ()
{
	int parents;

	parents = conn_length (NODE_PARENT, NODE_CONNECTED);

	return (parents < MAX_PARENTS);
}

/*****************************************************************************/

char *node_class_str (signed long klass)
{
	char *str;

	if (klass == -1)
		klass = NODE (ft_self)->class;

	if (klass & NODE_INDEX)
		str = "INDEX";
	else if (klass & NODE_PARENT)
		str = "PARENT";
	else if (klass & NODE_SEARCH)
		str = "SEARCH";
	else if (klass & NODE_CHILD)
		str = "CHILD";
	else if (klass & NODE_USER)
		str = "USER";
	else
		str = "NONE";

	return str;
}
