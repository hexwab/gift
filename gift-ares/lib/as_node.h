/*
 * $Id: as_node.h,v 1.1 2004/08/31 17:44:18 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_NODE_H_
#define __AS_NODE_H_

/*****************************************************************************/

typedef struct
{
	in_addr_t host;
	in_port_t port;

	time_t first_seen;    /* Time when we first heard of this node. */
	time_t last_seen;     /* Time we last heard of this node, either through
	                       * successful connect or report from other node. */
	time_t last_attempt;  /* Time we last tried to connect to node. */

	unsigned int attempts;  /* Number of times we have tried to connect to
	                         * this node. */
	unsigned int connects;  /* Number of times a connection attempt to this
	                         * node succeeded. */
	unsigned int reports;   /* Number of times this node was reported by
	                         * another node. */

	as_bool in_use;         /* TRUE if there is a session using this node.
	                         * Managed by node manager. */

	float weight;           /* Weight calculated from above data and used for
	                         * for sorting. */

} ASNode;

/*****************************************************************************/

/* Create new node. */
ASNode *as_node_create (in_addr_t host, in_port_t port);

/* Free node. */
void as_node_free (ASNode *node);

/*****************************************************************************/

#endif /* __AS_NODE_H_ */
