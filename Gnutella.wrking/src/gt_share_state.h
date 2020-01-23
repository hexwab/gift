/*
 * $Id: gt_share_state.h,v 1.2 2004/03/31 08:58:24 hipnod Exp $
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

#ifndef GIFT_GT_SHARE_STATE_H_
#define GIFT_GT_SHARE_STATE_H_

/******************************************************************************/

/*
 * Keep track of the "hidden" share status on the remote node.  To
 * disable sharing, the node is sent a HopsFlow message with a payload of 0,
 * telling it not to send us any queries.  If the HopsFlow message isn't
 * supported, the query handling code can simply drop the queries
 * (alternatively, the QRT could be manipulated, but that's more complicated).
 *
 * There are two variables to deal with: giftd may have disabled sharing, and
 * the plugin may have flow-controlled the remote node.  When either of these
 * conditions becomes true, the node is sent the HopsFlow message to disable
 * receipt of queries.  Only when both these conditions become false does the
 * node receive another HopsFlow with a payload of 8, allowing the node to send
 * us queries again.
 *
 * giftd's hidden state is tracked by gt_share_state.c.  Here, each node's
 * hidden state is tracked:
 */
struct gt_share_state
{
	BOOL  hidden;                  /* sharing is disabled for this node */
	BOOL  plugin_hidden;           /* plugin disabled it (flow control) */
};

/*****************************************************************************/

struct gt_share_state *gt_share_state_new    (void);
void                   gt_share_state_free   (struct gt_share_state *state);

/*****************************************************************************/

/* update the node about our sharing status, possibly by sending it a HopsFlow
 * message */
void                   gt_share_state_update (struct gt_node *node);

/* control share disabling on each individual node (as opposed to locally) */
void                   gt_share_state_hide   (struct gt_node *node);
void                   gt_share_state_show   (struct gt_node *node);

/*****************************************************************************/

void  gt_share_state_local_init    (void);
void  gt_share_state_local_cleanup (void);

/*****************************************************************************/

void  gnutella_share_hide (Protocol *p);
void  gnutella_share_show (Protocol *p);

/******************************************************************************/

#endif /* GIFT_GT_SHARE_STATE_H_ */
