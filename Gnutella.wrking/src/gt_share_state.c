/*
 * $Id: gt_share_state.c,v 1.2 2004/03/31 08:58:24 hipnod Exp $
 *
 * Copyright (C) 2004 giFT project (gift.sourceforge.net)
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
#include "gt_packet.h"
#include "gt_share_state.h"

/*****************************************************************************/

/* whether giftd has disabled shares */
static BOOL giftd_hidden             = FALSE;

/*****************************************************************************/

struct gt_share_state *gt_share_state_new (void)
{
	struct gt_share_state *state;

	if (!(state = malloc (sizeof(struct gt_share_state))))
		return NULL;

	/*
	 * Sharing may be disabled now globally, but it isn't for this
	 * gt_share_state until gt_share_state_update() is called.
	 */
	state->hidden        = FALSE;
	state->plugin_hidden = FALSE;

	return state;
}

void gt_share_state_free (struct gt_share_state *state)
{
	free (state);
}

static GtPacket *hops_flow_message (uint8_t ttl)
{
	GtPacket *pkt;

	if (!(pkt = gt_packet_vendor (GT_VMSG_HOPS_FLOW)))
		return NULL;

	gt_packet_put_uint8 (pkt, ttl);

	if (gt_packet_error (pkt))
	{
		gt_packet_free (pkt);
		return NULL;
	}

	return pkt;
}

static void toggle_sharing (GtNode *node, struct gt_share_state *state,
                            BOOL hidden)
{
	GtPacket *hops_disable;
	uint8_t   max_hops;

	/* regardless of whether the node receives our HospFlow, record whether
	 * sharing _should_ be disabled */
	state->hidden = hidden;

	if (hidden)
		max_hops = 0;
	else
		max_hops = 8;

	if (!(hops_disable = hops_flow_message (max_hops)))
		return;

	if (!dataset_lookupstr (node->hdr, "vendor-message"))
	{
		gt_packet_free (hops_disable);
		return;
	}

	GT->DBGSOCK (GT, GT_CONN(node), "sending HopsFlow(%d)", max_hops);

	gt_node_send (node, hops_disable);
	gt_packet_free (hops_disable);
}

void gt_share_state_update (GtNode *node)
{
	struct gt_share_state *state;

	assert (node->state == GT_NODE_CONNECTED);
	state  = node->share_state;

	if (state->hidden)
	{
		/* sharing disable, reenable it */
		if (!giftd_hidden && !state->plugin_hidden)
			toggle_sharing (node, state, FALSE);
	}
	else
	{
		/* sharing enabled, disable it */
		if (giftd_hidden || state->plugin_hidden)
			toggle_sharing (node, state, TRUE);
	}
}

/*****************************************************************************/

/*
 * gt_share_state_update() must have been called before calling either of
 * these.
 */

void gt_share_state_hide (GtNode *node)
{
	node->share_state->plugin_hidden = TRUE;
	gt_share_state_update (node);
}

void gt_share_state_show (GtNode *node)
{
	node->share_state->plugin_hidden = FALSE;
	gt_share_state_update (node);
}

/*****************************************************************************/

static GtNode *foreach_state (TCPC *c, GtNode *node, void *udata)
{
	gt_share_state_update (node);
	return NULL;
}

static void update_share_state (BOOL hidden)
{
	giftd_hidden = hidden;

	/*
	 * Ignore the command from giftd for ultrapeers. XXX: this isn't actually
	 * right, if we change status inbetween two of these message, we're
	 * screwed.
	 */
	if (GT_SELF->klass & GT_NODE_ULTRA)
		return;

	gt_conn_foreach (foreach_state, NULL,
	                 GT_NODE_ULTRA, GT_NODE_CONNECTED, 0);
}

void gnutella_share_hide (Protocol *p)
{
	if (giftd_hidden)
		return;

	update_share_state (TRUE);
}

void gnutella_share_show (Protocol *p)
{
	if (!giftd_hidden)
		return;

	update_share_state (FALSE);
}

/*****************************************************************************/

void gt_share_state_local_init (void)
{
	giftd_hidden = FALSE;
}

void gt_share_state_local_cleanup (void)
{
	giftd_hidden = FALSE;
}
