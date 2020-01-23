/*
 * $Id: ft_sharing.c,v 1.4 2003/06/09 15:28:29 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#include "ft_openft.h"

#include "ft_netorg.h"
#include "ft_conn.h"

#include "ft_search_db.h"

#include "ft_protocol.h"
#include "ft_sharing.h"

/*****************************************************************************/

FT_HANDLER (ft_child_request)
{
	uint16_t response = FALSE;

	/* user has requested to be our child, lets hope we're a search node ;) */
	if (!(FT_SELF->klass & FT_NODE_SEARCH))
		return;

	/* the final stage in negotiation
	 * NOTE: its here because it prevents a race condition in the child */
	if (packet->len > 0)
	{
		uint16_t reply;

		reply = ft_packet_get_uint16 (packet, TRUE);

		if (reply)
			ft_node_add_class (FT_NODE(c), FT_NODE_CHILD);
		else
			ft_node_remove_class (FT_NODE(c), FT_NODE_CHILD);

		return;
	}

	/*
	 * If either this node is already our child, or they have dirty records on
	 * our node, they should NOT be authorized.  They need to wait until
	 * this issue is resolved before they come back :)
	 */
	if (!ft_shost_get (FT_NODE(c)->ip))
	{
		/* if we have fewer than FT_MAX_CHILDREN, response is TRUE */
		if (ft_netorg_length (FT_NODE_CHILD, FT_NODE_CONNECTED) < FT_MAX_CHILDREN)
			response = TRUE;
	}

	/* reply to the child-status request */
	ft_packet_sendva (c, FT_CHILD_RESPONSE, 0, "h", response);
}

FT_HANDLER (ft_child_response)
{
	uint16_t response;

	if (!(FT_NODE(c)->klass & FT_NODE_SEARCH))
		return;

	response = ft_packet_get_uint16 (packet, TRUE);

	/* they refused our request */
	if (!response)
	{
		FT->DBGSOCK (FT, c, "request refused");
		ft_node_remove_class (FT_NODE(c), FT_NODE_PARENT);
		return;
	}

	/* figure out if we still need them */
	if (!ft_conn_need_parents () || FT_NODE(c)->version < OPENFT_0_0_9_6)
	{
		/* gracefully inform this node that we have decided not to accept
		 * them as our parent */
		ft_packet_sendva (c, FT_CHILD_REQUEST, 0, "h", FALSE);
		return;
	}

	/* accept */
	ft_packet_sendva (c, FT_CHILD_REQUEST, 0, "h", TRUE);

	if (FT_NODE(c)->klass & FT_NODE_PARENT)
		return;

	ft_node_add_class (FT_NODE(c), FT_NODE_PARENT);

	/* we have a new parent, submit our shares and once complete
	 * ft_share_local_submit will notify them of our sharing eligibility so
	 * that they may report it to users searching for our files */
	ft_share_local_submit (c);
}

/*****************************************************************************/

static int is_child (TCPC *c)
{
	if (!(FT_SELF->klass & FT_NODE_SEARCH))
		return FALSE;

	return (FT_NODE(c)->klass & FT_NODE_CHILD);
}

FT_HANDLER (ft_addshare_request)
{
	uint32_t       size;
	char          *mime;
	unsigned char *md5;
	char          *path;
	char          *meta_key, *meta_value;
	Dataset       *meta = NULL;

	if (!is_child (c))
		return;

	size = ft_packet_get_uint32 (packet, TRUE);
	mime = ft_packet_get_str    (packet);
	md5  = ft_packet_get_ustr   (packet, 16);
	path = ft_packet_get_str    (packet);

	if (!mime || !md5 || !path || size == 0)
		return;

	while ((meta_key = ft_packet_get_str (packet)))
	{
		if (!(meta_value = ft_packet_get_str (packet)))
			break;

		dataset_insert (&meta, meta_key, STRLEN_0 (meta_key), meta_value, 0);
	}

	ft_share_add (FT_SESSION(c)->verified, FT_NODE(c)->ip, FT_NODE(c)->port,
	              FT_NODE(c)->http_port, FT_NODE(c)->alias,
	              (off_t)size, md5, mime, path, meta);

	dataset_clear (meta);
}

FT_HANDLER (ft_addshare_response)
{
}

/*****************************************************************************/

FT_HANDLER (ft_remshare_request)
{
	unsigned char *md5;

	if (!is_child (c))
		return;

	/* remove all shares
	 * NOTE: this keeps the user authorized as a child assuming the shares
	 * will be resynced soon */
	if (!ft_packet_length (packet))
	{
		ft_shost_remove (FT_NODE(c)->ip);
		return;
	}

	if (!(md5 = ft_packet_get_ustr (packet, 16)))
		return;

	ft_share_remove (FT_NODE(c)->ip, md5);
}

FT_HANDLER (ft_remshare_response)
{
}

/*****************************************************************************/

static int submit_digest_index (FTNode *node, TCPC *child)
{
	unsigned long shares = 0;
	double        size   = 0.0;        /* MB */

	if (!ft_shost_digest (FT_NODE(child)->ip, &shares, &size, NULL))
		return FALSE;

	/* submit the digest */
	ft_packet_sendva (FT_CONN(node), FT_STATS_REQUEST, 0, "hIll",
	                  2 /* SUBMIT DIGEST */,
	                  FT_NODE(child)->ip, shares, (unsigned long)size);

	return TRUE;
}

/* send this users share digest to all index nodes */
static void submit_digest (TCPC *c)
{
	ft_netorg_foreach (FT_NODE_INDEX, FT_NODE_CONNECTED, 0,
	                   FT_NETORG_FOREACH(submit_digest_index), c);
}

FT_HANDLER (ft_modshare_request)
{
	uint16_t trans_comp;
	uint32_t avail;

	if (!is_child (c))
		return;

	trans_comp = ft_packet_get_uint16 (packet, TRUE);
	avail      = ft_packet_get_uint32 (packet, TRUE);

	if (trans_comp)
	{
		ft_search_db_sync (ft_shost_get (FT_NODE(c)->ip));
		submit_digest (c);
	}

	ft_shost_avail (FT_NODE(c)->ip, (unsigned long)avail);
}

FT_HANDLER (ft_modshare_response)
{
}

/*****************************************************************************/

/* multipurpose function.  may either submit a users digest to an INDEX node
 * or retrieve the total shares an INDEX node is aware of */
FT_HANDLER (ft_stats_request)
{
	uint16_t request;

	/* this command is for INDEX nodes only! */
	if (!(FT_SELF->klass & FT_NODE_INDEX))
		return;

	request = ft_packet_get_uint16 (packet, TRUE);

	switch (request)
	{
	 case 1: /* RETRIEVE INFO */
		{
			unsigned long users  = 0;
			unsigned long shares = 0;
			double        size   = 0.0; /* GB */

			ft_stats_get (&users, &shares, &size);

			ft_packet_sendva (c, FT_STATS_RESPONSE, 0, "lll",
			                  users, shares, (unsigned long) size);
		}
		break;
	 case 2: /* SUBMIT USER SHARES REPORT */
		{
			uint32_t user;             /* user who owns these files */
			uint32_t shares;
			uint32_t size;             /* MB */

			user   = ft_packet_get_ip     (packet);
			shares = ft_packet_get_uint32 (packet, TRUE);
			size   = ft_packet_get_uint32 (packet, TRUE);

			/* handle processing of this data */
			ft_stats_add (FT_NODE(c)->ip, user, shares, size);
		}
		break;
	 case 3: /* REMOVE USER SHARES REPORT */
		{
			uint32_t user;

			user = ft_packet_get_ip (packet);

			ft_stats_remove (FT_NODE(c)->ip, user);
		}
		break;
	 default:
		break;
	}
}

FT_HANDLER (ft_stats_response)
{
	unsigned long users;
	unsigned long shares;
	unsigned long size; /* GB */

	/* ignore this data from a non-index node */
	if (!(FT_NODE(c)->klass & FT_NODE_INDEX))
		return;

	users  = ft_packet_get_uint32 (packet, TRUE);
	shares = ft_packet_get_uint32 (packet, TRUE);
	size   = ft_packet_get_uint32 (packet, TRUE);

	/* set this index nodes share stats */
	FT_SESSION(c)->stats.users  = users;
	FT_SESSION(c)->stats.shares = shares;
	FT_SESSION(c)->stats.size   = size;
}
