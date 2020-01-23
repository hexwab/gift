/*
 * $Id: ft_sharing.c,v 1.5 2003/06/24 19:57:20 jasta Exp $
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

#include "md5.h"

/*****************************************************************************/

static BOOL is_child (TCPC *c)
{
	if (!(FT_SELF->klass & FT_NODE_SEARCH))
		return FALSE;

	return (FT_NODE(c)->klass & FT_NODE_CHILD);
}

/*****************************************************************************/

FT_HANDLER (ft_child_request)
{
	/* user has requested to be our child, lets hope we're a search node ;) */
	if (!(FT_SELF->klass & FT_NODE_SEARCH))
		return;

	if (FT_SELF->klass & FT_NODE_CHILD)
	{
		FT->DBGSOCK (FT, c, "uhh, someone requested to be our child when they"
		                    "already were?");
		return;
	}

	/*
	 * Child negotiation is a two-stage process.  First a request is made
	 * here, and we give an initial response.  The soon-to-be child then
	 * sends a new request indicating whether or not they accept, which
	 * completes the process.
	 *
	 * NOTE: This exists to prevent a race condition in both the child and
	 * the parent, although I see many better ways to accomplish the same
	 * goals.  Perhaps they will be explored at a later date.
	 */
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
	else
	{
		BOOL response = TRUE;          /* accept or deny? */

		/* this node has some unfinished business with their database */
		if (FT_SESSION(c)->search_db)
			response = FALSE;
		else
		{
			int n = ft_netorg_length (FT_NODE_CHILD, FT_NODE_CONNECTED);

			/*
			 * NOTE: this is _NOT_ handled gracefully by the network and
			 * absolutely needs to be!  This cannot be left like this.
			 */
			if (n >= FT_MAX_CHILDREN)
			{
				FT->DBGSOCK (FT, c, "max child count reached, sigh");
				response = FALSE;
			}
		}

		ft_packet_sendva (c, FT_CHILD_RESPONSE, 0, "h", response);
	}
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

FT_HANDLER (ft_child_prop)
{
	uint32_t avail;

	if (!(is_child (c)))
		return;

	avail = ft_packet_get_uint32 (packet, TRUE);

	FT_SESSION(c)->avail = avail;
}

/*****************************************************************************/

FT_HANDLER (ft_share_sync_begin)
{
	FT->DBGSOCK (FT, c, "opening share database");

	/* really all this does is create the FT_SEARCH_DB(...) object */
	ft_search_db_open (FT_NODE(c));
}


static BOOL submit_digest_index (FTNode *node, FTNode *child)
{
	FTPacket *pkt;

	if (!(pkt = ft_packet_new (FT_STATS_DIGEST_ADD, 0)))
		return FALSE;

	ft_packet_put_ip     (pkt, child->ip);
	ft_packet_put_uint32 (pkt, (uint32_t)FT_SEARCH_DB(child)->shares, TRUE);
	ft_packet_put_uint32 (pkt, (uint32_t)FT_SEARCH_DB(child)->size,   TRUE);

	ft_packet_send (FT_CONN(node), pkt);

	return TRUE;
}

FT_HANDLER (ft_share_sync_end)
{
	/* i would like to assert this, but unfortunately it can occur from a
	 * malicious user so we have to be graceul about it */
	if (!FT_SEARCH_DB(FT_NODE(c)))
		return;

	/* again, not really */
	FT->DBGSOCK (FT, c, "closing share database, %lu (%.02fGB)",
	             FT_SEARCH_DB(FT_NODE(c))->shares,
	             FT_SEARCH_DB(FT_NODE(c))->size / 1024.0);

	/* this does nothing by default :) */
	ft_search_db_sync (FT_NODE(c));

	/* send a new digest report to index nodes */
	ft_netorg_foreach (FT_NODE_INDEX, FT_NODE_CONNECTED, 0,
	                   FT_NETORG_FOREACH(submit_digest_index), FT_NODE(c));
}

/*****************************************************************************/

FT_HANDLER (ft_share_add_request)
{
	Share          share;
	uint32_t       size;
	char          *mime;
	unsigned char *md5;
	char          *path;
	char          *meta_key, *meta_value;
	BOOL           ret;

	if (!(is_child (c)))
		return;

	md5  = ft_packet_get_ustr   (packet, 16);
	path = ft_packet_get_str    (packet);
	mime = ft_packet_get_str    (packet);
	size = ft_packet_get_uint32 (packet, TRUE);

	/* share_complete() should take care of this for us, but it is too
	 * shitty in its current state */
	if (!md5 || !path || !mime || size == 0)
		return;

	/* we can't really avoid copying the path because of this interface...
	 * damnit */
	if (!(ret = share_init (&share, path)))
	{
		FT->DBGSOCK (FT, c, "unable to initialize share object");
		return;
	}

	/* insert the md5 so that the search db code can lookup without a copy */
	share_set_hash (&share, "MD5", md5, 16, FALSE);

	/* avoid using share_set_mime on purpose as it will attempt to
	 * avoid a copy by using mime_type_lookup, which we don't actually need */
	share.mime = mime;
	share.size = size;

	while ((meta_key = ft_packet_get_str (packet)))
	{
		if (!(meta_value = ft_packet_get_str (packet)))
			break;

		share_set_meta (&share, meta_key, meta_value);
	}

	if (!(ret = ft_search_db_insert (FT_NODE(c), &share)))
		ft_packet_sendva (c, FT_SHARE_ADD_ERROR, 0, "Ss", md5, 16, "UNKNOWN");

	share_finish (&share);
}

FT_HANDLER (ft_share_add_error)
{
	Share         *share;
	unsigned char *md5;
	char          *errstr;

	if (!(md5 = ft_packet_get_ustr (packet, 16)))
		return;

	if (!(share = FT->share_lookup (FT, SHARE_LOOKUP_HASH, "MD5", md5, 16)))
	{
		FT->DBGFN (FT, "cannot lookup %s", md5_fmt (md5));
		return;
	}

	errstr = ft_packet_get_str (packet);

	FT->DBGSOCK (FT, c, "insert err: %s: %s",
	             md5_fmt (md5), STRING_NOTNULL(errstr));
}

/*****************************************************************************/

FT_HANDLER (ft_share_remove_request)
{
	unsigned char *md5;

	if (!is_child (c))
		return;

	/*
	 * This is a special condition which indicates that the entire database
	 * should be truncated under the assumption that a new database will be
	 * inserted individually.  I am not certain whether or not this feature
	 * is still being used by this implementation, but it is still worth
	 * honoring in the protocol.
	 */
	if (ft_packet_length (packet) == 0)
	{
		ft_search_db_remove_host (FT_NODE(c));
		return;
	}
	else
	{
		/* normal operation: remove a single share */
		if (!(md5 = ft_packet_get_ustr (packet, 16)))
			return;

		ft_search_db_remove (FT_NODE(c), md5);
	}
}

FT_HANDLER (ft_share_remove_error)
{
	/* not used by this implementation */
}
