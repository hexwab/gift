/* giFT OpenNap
 * Copyright (C) 2003 Tilman Sauerbeck <tilman@code-monkey.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __OPN_NODE_H
#define __OPN_NODE_H

typedef enum {
	OPN_NODE_STATE_OFFLINE,
	OPN_NODE_STATE_ONLINE
} OpnNodeState;

typedef struct {
	in_addr_t ip;
	in_port_t port;
	OpnNodeState state;
} OpnNode;

typedef struct {
	TCPC *con; /* connection for napigator */
	List *nodes;
} OpnNodeList;

OpnNode *opn_node_new(in_addr_t ip, in_port_t port);
void opn_node_free(OpnNode *node);

OpnNodeList *opn_nodelist_new();
void opn_nodelist_free(OpnNodeList *nodelist);

void opn_nodelist_node_add(OpnNodeList *nodelist, OpnNode *node);
void opn_nodelist_node_remove(OpnNodeList *nodelist, OpnNode *node);

BOOL opn_nodelist_save(OpnNodeList *nodelist);
BOOL opn_nodelist_load(OpnNodeList *nodelist);

void opn_nodelist_refresh(OpnNodeList *nodelist);

#endif

