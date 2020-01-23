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

#ifndef __FT_NODE_H
#define __FT_NODE_H

/*****************************************************************************/

/**
 * @file ft_node.h
 *
 * @brief Holds the main node structure and supporting functions.
 *
 * The FTNode structure is used primarily for storing and accessing data
 * found in the nodes cache while OpenFT is active.  Keep in mind that a
 * great deal of this protocol utilizes this structure, but do not include
 * their support functions here.
 */

/*****************************************************************************/

#include "ft_session.h"
#include "queue.h"

/*****************************************************************************/

/**
 * Connection state for the underlying socket implementation.
 */
typedef enum
{
	NODE_DISCONNECTED = 0x01,          /**< for whatever reason we have
	                                    *   connection information about this
	                                    *   node, but we are not currently
	                                    *   connected */
	NODE_CONNECTING   = 0x02,          /**< actively connecting and/or
	                                    *   handshaking an OpenFT connection */
	NODE_CONNECTED    = 0x04,          /**< actively connected and a
	                                    *   "near-complete" OpenFT session
	                                    *   has been established */
} FTNodeState;

/**
 * @brief OpenFT-specific node classification.
 *
 * OpenFT classifications are NOT implicitly exclusive, that is, if a node is
 * selected to be a search node, it does not necessarily disqualify that node
 * from being an index node as well.  This is rather apparent when you
 * observe the method of testing for node statuses and more importantly the
 * actual class values in this enumerated list.  Please note that not all
 * class values listed here are actually classifications, rather some are
 * class modifiers.  See the documentation below (or our actual OpenFT
 * documentation) for more details.
 */
typedef enum
{
	/**
	 * @name Classifications
	 */
	NODE_USER   = 0x01,                /**< base class for all OpenFT nodes */
	NODE_SEARCH = 0x02,                /**< node responsible for indexing and
	                                    *   searching peer files */
	NODE_INDEX  = 0x04,                /**< node responsible for indexing the
	                                    *   openft network as a whole (not
	                                    *   files!) and guiding nodes through
	                                    *   node clusters and network
	                                    *   splits */

	/**
	 * @name Class Modifiers
	 *
	 * Special class values that modify the class of the node that it is
	 * applied to.  This information is not maintained in the nodes cache as
	 * it depends on the current session state of this node connection.
	 */
	NODE_CHILD  = 0x100,               /**< node is sharing files to 'self'.
	                                    *   modifies a user node given that
	                                    *   our current node is search. */
	NODE_PARENT = 0x200,               /**< node is indexing our files.
	                                    *   modifies a search node. */
} FTNodeClass;

/**
 * Errno-style error reporting system so that log lines actually show
 * something useful.
 */
typedef enum
{
	FT_ERROR_SUCCESS = 0,              /**< Nothing wrong */
	FT_ERROR_VERMISMATCH,              /**< Incompatible OpenFT versions  */
	FT_ERROR_UNKNOWN,                  /**< Undescribed error */
} FTNodeError;

/**
 * Main OpenFT node structure.  Almost all session-specific node data is held
 * in FTSession, which is directly associated w/ this structure.  Exceptions
 * being child, parent, etc class relationships, versioning information, and
 * aliases.  Also, there is a queue structure here that it is utilized as a
 * sort of temporary buffer prior to the session's construction, and thus
 * cannot use the session for that reason.
 */
typedef struct _ft_node
{
	ft_uint32      version;            /**< OpenFT ver, see ft_version.c */

	time_t         last_session;       /**< Time the last session was closed */
	time_t         uptime;             /**< Total node uptime, this is not
	                                    *   a consecutive calculation like a
	                                    *   regular UNIX uptime */

	FTSession     *session;            /**< Session-specific node data, this
	                                    *   includes (among other things)
	                                    *   handshaking state, connection
	                                    *   information, and non-persisting
	                                    *   node data */
	List          *session_queue;      /**< List of all packets queued for
	                                    *   delivery when the session has
	                                    *   been activated */

	in_addr_t      ip;                 /**< IPv4 address of this node */
	unsigned short port;               /**< OpenFT port, 0 if firewalled */
	unsigned short http_port;          /**< HTTP port */
	char          *alias;              /**< Basically nickname */

	FTNodeState    state;              /**< Connection state */
	FTNodeClass    klass;              /**< Node class */

	FTNodeError    lasterr;            /**< Like errno, but for node
	                                    *   connections */
	char          *lasterr_msg;        /**< Simple message to append
	                                    *   to the error reporting */
} FTNode;

/**
 * Simple helper macro for accessing FTNode data given a Connection object.
 * This macro assumes that the supplied connection is actually an OpenFT node
 * connection.
 */
#define FT_NODE(c) ((FTNode *)c->data)

/**
 * Safely access the connection associated with a particular node.
 */
#define FT_CONN(node) (node ? (node->session ? node->session->c : NULL) : NULL)

/*****************************************************************************/

/**
 * Forcefully create a new FTNode structure without registering in a
 * centralized list and/or requesting a lookup from that list.  This should
 * be used sparingly and is provided only for instances where an API calls
 * for a node structure, but one with the necessary information is not
 * provided or not desired to be registered.
 */
FTNode *ft_node_new (in_addr_t ip);

/**
 * Free a node returned from node_new.  Do not mix usage of node registration
 * with this, as the results will be quite catastrophic.  Make sure you see
 * ::node_unregister before you consider usage.
 */
void ft_node_free (FTNode *node);

/**
 * Register a node based on ip addresses as uniqueness.  This function is
 * similar to a constructor in that it will create a new object and return if
 * there is currently no registered node from this ip address.  In the event
 * that a node has already been registered, it's associated structure will be
 * returned.  Naturally, it is safe to assume registered nodes are automatically managed in some data structure.
 *
 * @param ip         IPv4 address to register.
 * @param port
 * @param http_port
 * @param klass      OpenFT classification.
 * @param vitality   Time that the last session ended.
 * @param uptime     Total time this node has been connected to us.  Persists.
 * @param version    Remote node version.  See ::ft_version for more
 *                   information.
 *
 * @return Either a new FTNode object or a pointer to an existing one.  NULL
 *         will be returned on an unrecoverable error.
 */
FTNode *ft_node_register_full (in_addr_t ip,
                               unsigned short port,
                               unsigned short http_port,
                               FTNodeClass klass,
                               time_t vitality,
                               time_t uptime,
                               ft_uint32 version);

/**
 * Wrapper around ft_node_register_full.  This function is recommended over
 * ::ft_node_register_full, except that it will be unable to take advantage
 * of the sorted insertion functionality.
 */
FTNode *ft_node_register (in_addr_t ip);

/**
 * Unregister a previously registered node given its object pointer.  The
 * data contained within \em node (and \em node itself) will be freed upon
 * calling this function.  Do not use this function on a node that has not
 * yet been registered, see ::node_free instead.
 *
 * @param node  Registered node returned from node_register.
 */
void ft_node_unregister (FTNode *node);

/**
 * Unregister a node given an IP address.  Obviously, a lookup is required
 * here, so usage is discouraged when an FTNode structure is already
 * available.
 */
void ft_node_unregister_ip (in_addr_t ip);

/*****************************************************************************/

/**
 * Set the current error status.
 *
 * @param node
 * @param err     Error family.
 * @param errtxt  Optional error text.  May specify NULL.
 */
void ft_node_err (FTNode *node, FTNodeError err, char *errtxt);

/**
 * Return the last error value formatted for display.  This is currently
 * only utilized within ft_node.c, but may be useful elsewhere so it is
 * provided publicly.
 *
 * @return Pointer to a static buffer containing the formatted error string.
 */
char *ft_node_geterr (FTNode *node);

/*****************************************************************************/

#if 0
/**
 * Dynamically change the IP address associated with this node.  This is
 * useful when you want to transfer over all the non-session related data
 * from one node structure to another without manual construction and
 * destruction.
 */
void ft_node_set_ip (FTNode *node, in_addr_t ip);
#endif

/**
 * Set the OpenFT port (not HTTP).  Note that if you pass 0 here, it will be
 * assumed that you have flagged this node as firewalled (unavailable for
 * incoming connections).
 */
void ft_node_set_port (FTNode *node, unsigned short port);

/**
 * Set the HTTP port.  Supplying 0 here will no-op.
 */
void ft_node_set_http_port (FTNode *node, unsigned short http_port);

/**
 * Set the node alias.  Pre-processing on the supplied string will be used so
 * that the alias matches the protocol-defined standard for aliases.
 *
 * @return Processed node alias.  The memory address will not match the input
 *         parameter.
 */
char *ft_node_set_alias (FTNode *node, char *alias);

/*****************************************************************************/

/**
 * Queue a packet for delivery when this node's session has been established.
 * See ::ft_packet_sendto for more details on how this is used.
 */
void ft_node_queue (FTNode *node, FTPacket *packet);

/*****************************************************************************/

/**
 * Set the node connection state.  You should note that this is not a flat
 * accessor for the state member variable of the FTNode structure.  Instead,
 * this function attempts to activate certain state-based actions given the
 * change noted by \em state.  It is safe to set the same state twice.
 */
void ft_node_set_state (FTNode *node, FTNodeState state);

/**
 * Set the node classification.  See ::ft_node_set_state for details on how
 * this affects OpenFT's behaviour.
 */
void ft_node_set_class (FTNode *node, FTNodeClass klass);

/**
 * Add a class non-destructively.  That is, you can add a new class without
 * affecting any previously set class values.  This also indirectly affects
 * how class changes are handled internally (in contrast with set_class).
 */
void ft_node_add_class (FTNode *node, FTNodeClass klass);

/**
 * Remove a class without affecting other [unrelated] data.  See
 * ::ft_node_add_class for more details.
 */
void ft_node_remove_class (FTNode *node, FTNodeClass klass);

/**
 * Accessor for node->class that allows for filtering of session-specific
 * class modifiers so that the logic doesn't need to be hardcoded elsewhere
 * in the code.
 *
 * @param node
 * @param session_info  If TRUE, NODE_CHILD and NODE_PARENT will be left in.
 *
 * @return This node's classification, or 0 on error.
 */
FTNodeClass ft_node_class (FTNode *node, int session_info);

/**
 * Format the supplied class as a human readable string.
 *
 * @param klass
 *
 * @return Static pointer representing the class string.
 */
char *ft_node_classstr (FTNodeClass klass);

/**
 * Similar to ::ft_node_classstr, but utilized for connection states.
 */
char *ft_node_statestr (FTNodeState state);

/*****************************************************************************/

/**
 * Format a node structure for display in a log line.  This is a simple
 * helper so that you do not need to manually call net_ip_str and friends.
 *
 * @param Pointer to the staticly formatted string.
 */
char *ft_node_fmt (FTNode *node);

/**
 * Wrapper for ::ft_node_user_host.
 */
char *ft_node_user (FTNode *node);

/**
 * Return the user which should be returned to giFT for the given node.
 *
 * @param host
 * @param alias
 *
 * @return Pointer to static memory holding the formatted user.
 */
char *ft_node_user_host (in_addr_t host, char *alias);

/**
 * Determines whether or not the supplied node is firewalled or not through
 * the port logic.  This, at some later date, may change and thus the
 * abstraction exists.
 *
 * @return If TRUE, the node is considered firewalled and unavailable for
 *         direct contact.
 */
int ft_node_fw (FTNode *node);

/*****************************************************************************/

#endif /* __FT_NODE_H */
