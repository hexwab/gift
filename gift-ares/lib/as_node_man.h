/*
 * $Id: as_node_man.h,v 1.3 2004/09/05 12:31:02 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_NODE_MAN_H_
#define __AS_NODE_MAN_H_

/*****************************************************************************/

typedef struct
{
	List *nodes;         /* Sorted list of all nodes */
	ASHashTable *index;  /* Index of list links keyed by ip */

	time_t oldest_first_seen; /* The earliest we firts saw any node. Used in
	                           * node weight calculation. */

	time_t oldest_last_seen;  /* The earliest we last saw any node. Used in
	                           * node weight calculation. */

} ASNodeMan;

/*****************************************************************************/

/* Create new node manager. */
ASNodeMan *as_nodeman_create ();

/* Free node manager. */
void as_nodeman_free (ASNodeMan *man);

/* Free all nodes. */
void as_nodeman_empty (ASNodeMan *man);

/* Print all nodes to log. */
void as_nodeman_dump (ASNodeMan *man);

/*****************************************************************************/

/* Get next best node for connecting. Sets node->last_attempt to now and
 * increments node->attempts. Reinserts node at the correct position so the
 * next call will not return the same node. Sets node->in_use to TRUE and
 * won't return nodes which are in use. If there are no unused, eligible nodes
 * NULL is returned.
 */
ASNode *as_nodeman_next (ASNodeMan *man);

/* Update node with specified ip when connection succeeded. Updates
 * node->last_seen and node->connects. Returns FALSE if node is not in list.
 */
as_bool as_nodeman_update_connected (ASNodeMan *man, in_addr_t host);

/* Update node with specified ip when connection failed. May remove node from
 * internal list if it is deemed useless. Returns FALSE if node is not in
 * list. Sets node->in_use to FALSE.
 */
as_bool as_nodeman_update_failed (ASNodeMan *man, in_addr_t host);

/* Update node with specified ip on disconnect. Updates node->last_seen. May
 * remove node if it is deemed useless. Returns FALSE if node is not in list.
 * Sets node->in_use to FALSE.
 */
as_bool as_nodeman_update_disconnected (ASNodeMan *man, in_addr_t host);

/* Update node with specified ip when it is reported by other supernodes. If 
 * node is not in list it is added. Updates node->last_seen, node->reports and
 * node->port.
 */
void as_nodeman_update_reported (ASNodeMan *man, in_addr_t host,
                                 in_port_t port);

/*****************************************************************************/

/* Load nodes from file. */
as_bool as_nodeman_load (ASNodeMan *man, const char *file);

/* Save nodes to file */
as_bool as_nodeman_save (ASNodeMan *man, const char *file);

/*****************************************************************************/

#endif /* __AS_NODE_MAN_H_ */
