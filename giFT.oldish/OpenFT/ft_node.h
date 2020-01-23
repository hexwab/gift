/*
 * ft_node.h
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

#ifndef __FT_NODE_H__
#define __FT_NODE_H__

/*****************************************************************************/

#include "ft_session.h"
#include "queue.h"

/**************************************************************************/

/*
 * Main node structure
 *
 * NOTE:
 * Most of the data contained here will be logically merged into
 * ft_session.c over time.
 */
typedef struct _ft_node
{
	/* last known connected timestamp */
	time_t         last_session;
	time_t         uptime;

	/* openft revision */
	ft_uint32      version;

	FTSession     *session;

	/* capabilities list */
	Dataset       *cap;

	/* SEARCH node only - temporary shares transaction stuff */
	char          *shares_path;
	FILE          *shares_file;

	/* INDEX node only - holds users, files, size */
	FTStats        stats;

	/* information used to get back to the given host */
	in_addr_t      ip;
	unsigned short port;
	unsigned short http_port;

	char          *alias;              /* nickname, basically */

	/* host authentication */
	Connection    *port_verify;
	Connection    *http_port_verify;

	/* streams */
	Dataset       *streams_recv;
	Dataset       *streams_send;

	/* NOTE: keeping these close helps compilers optimize the size */

	unsigned char  verified : 1;       /* both ports have been verified */
	unsigned char  incoming : 1;       /* this node connected to us */
	unsigned char  firewalled : 1;     /* port is 0 */

	/* node list transaction information */
	unsigned char  sent_list : 1;
	unsigned char  recv_list : 1;

	/* each time a heartbeat is sent, increment, each time it is responded to,
	 * decrement.  if heartbeat reaches 5, disconnect */
	unsigned char  heartbeat : 4;

	/* temporarily ignores nodes that are misbehaving (version mismatch or
	 * whatever) */
	unsigned char  disconnect_cnt : 4;

	/* current state of the given connection (TODO - IDLE cannot be properly
	 * implemented in this way) */
	enum
	{
		NODE_DISCONNECTED  = 0x00,     /* functionless node */
		NODE_CONNECTING    = 0x01,     /* pending */
		NODE_CONNECTED     = 0x02,     /* first packet is seen */
		NODE_IDLE          = 0x04,     /* after handshake */
	} state;

	/* node classification that this connection is communicating with
	 * (bitwise OR) */
	enum
	{
		NODE_NONE   = 0x00,
		NODE_USER   = 0x01,
		NODE_SEARCH = 0x02,
		NODE_INDEX  = 0x04,
		NODE_CHILD  = 0x08,            /* node is sharing files to 'self' */
		NODE_PARENT = 0x10,            /* node has 'self' files shared */
	} class;

	/* file descriptor */
	int            fd;

	/* reference to our select loop place */
	int            input;
} Node;

#define FT_NODE(c) ((Node *)c->data)

/*****************************************************************************/

Connection *node_register   (in_addr_t ip, signed short port,
                             signed short http_port, unsigned short klass,
                             int outgoing);
Connection *node_new        (int fd);
void        node_free       (Connection *c);
void        node_remove     (Connection *c);
void        node_disconnect (Connection *c);

/*****************************************************************************/

void node_update_cache   ();
int  node_maintain_links (void *udata);

/*****************************************************************************/

void node_state_set    (Connection *c, unsigned short state);
void node_class_set    (Connection *c, unsigned short klass);
void node_class_add    (Connection *c, unsigned short klass);
void node_class_remove (Connection *c, unsigned short klass);
void node_conn_set     (Connection *c, in_addr_t ip, signed long port,
                        signed long http_port, char *alias);

char *node_user (in_addr_t host, char *alias);

/**************************************************************************/

#endif /* __FT_NODE_H__ */
