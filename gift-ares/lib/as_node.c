/*
 * $Id: as_node.c,v 1.2 2004/08/31 22:05:58 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* Create new node. */
ASNode *as_node_create (in_addr_t host, in_port_t port)
{
	ASNode *node;

	if (!(node = malloc (sizeof (ASNode))))
		return NULL;

	node->host = host;
	node->port = port;

	node->first_seen   = 0;
	node->last_seen    = 0;
	node->last_attempt = 0;

	node->attempts = 0;
	node->connects = 0;
	node->reports  = 0;

	node->in_use = FALSE;

	node->weight = 0.0;

	return node;
}

/* Free node. */
void as_node_free (ASNode *node)
{
	if (!node)
		return;

	free (node);
}

/*****************************************************************************/
