/*
 * $Id: ft_push.c,v 1.12 2003/11/02 12:09:08 jasta Exp $
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

#include "ft_transfer.h"
#include "ft_http_client.h"

#include "ft_protocol.h"

#include "ft_push.h"

/*****************************************************************************/

FT_HANDLER (ft_push_request)
{
	in_addr_t  ip;
	in_port_t  port;
	char      *request;

	ip      = ft_packet_get_ip     (packet);
	port    = ft_packet_get_uint16 (packet, TRUE);
	request = ft_packet_get_str    (packet);

	/* sanity check on the data received */
	if (!request)
		return;

	/* ip || port == 0 in this instance means that we are to send this file
	 * back to the search node contacting us here */
	if (ip == 0 || port == 0)
	{
		ip   = FT_NODE_INFO(c)->host;
		port = FT_NODE_INFO(c)->port_http;
	}

	/* make an outgoing http connection and advertise that we can fulfill
	 * the push request */
	ft_http_client_push (ip, port, request);
}

FT_HANDLER (ft_push_fwd_request)
{
	FTPacket *fwd;
	FTNode   *fwdto;
	in_addr_t ip;
	char     *request;

	ip      = ft_packet_get_ip  (packet);
	request = ft_packet_get_str (packet);

	if (ip == 0 || !request)
	{
		FT->DBGSOCK (FT, c, "invalid push forward request");
		return;
	}

	FT->DBGSOCK (FT, c, "push forward to %s: %s", net_ip_str (ip), request);

	/*
	 * See if we actually have a connection to this peer.  If not, we obviously
	 * don't have them as our child and we can't satisfy this push request.
	 */
	if (!(fwdto = ft_netorg_lookup (ip)) || !FT_CONN(fwdto))
	{
		ft_packet_sendva (c, FT_PUSH_FWD_RESPONSE, 0, "Ihs",
		                  ip, FALSE, "NO_ROUTE_REMOTE");

		return;
	}

	/*
	 * Check to see if the downloading party can actually receive connections
	 * on their HTTP port.
	 */
	if (ft_node_fw (FT_NODE(c)))
	{
		ft_packet_sendva (c, FT_PUSH_FWD_RESPONSE, 0, "Ihs",
		                  ip, FALSE, "NO_ROUTE_LOCAL");

		return;
	}

	/*
	 * Check to see if the user we are connected to for forwarding is
	 * actually our child.  We should still honor the request if we can, but
	 * a warning would be nice.
	 */
	if (!(fwdto->ninfo.klass & FT_NODE_CHILD))
		FT->DBGSOCK (FT, c, "non-child forward request?");

	if ((fwd = ft_packet_new (FT_PUSH_REQUEST, 0)))
	{
		/* reconstruct the complete request using the directly connected peers
		 * address information */
		ft_packet_put_ip     (fwd, FT_NODE_INFO(c)->host);
		ft_packet_put_uint16 (fwd, FT_NODE_INFO(c)->port_http, TRUE);

		/* put the original data back on */
		ft_packet_put_str    (fwd, request);

		/*
		 * Pass this data along to the user who will be actually sending the
		 * file to the user making this request.
		 *
		 * NOTE: No further relayed communication will take place.  If
		 * successful, the node will connect back to the user and advertise
		 * the file, otherwise it will just be dropped off into oblivion
		 * right here. Life is rough, eh? :)
		 */
		ft_packet_send (FT_CONN(fwdto), fwd);
	}

	/* notify the remote peer that the forward request was successful */
	ft_packet_sendva (c, FT_PUSH_FWD_RESPONSE, 0, "Ih", ip, TRUE);
}

static BOOL nuke_source (FTNode *parent, in_addr_t user, FTTransfer *xfer)
{
	Transfer         *t;
	Source           *s;
	struct ft_source *src;

	/* access transfer data */
	t = ft_transfer_get_transfer (xfer);
	assert (t != NULL);

	s = ft_transfer_get_source (xfer);
	assert (s != NULL);

	src = s->udata;
	assert (src != NULL);

	/* avoid operating on a source which isnt from the user we are
	 * trying to remove */
	if (src->host != user)
		return FALSE;

	/* avoid operating on a source which wasnt firewalled to begin with */
	if (src->search_host == 0)
		return FALSE;

	/* avoid operating on a source which says its parent is someone other
	 * than the node who sent the error reply */
	if (src->search_host != parent->ninfo.host)
		return FALSE;

	/*
	 * Call for giFT to destroy this source, which will call our
	 * FT->download_stop and friends in the process for cleanup...
	 */
	FT->DBGFN (FT, "removing dead source: %s", s->url);
	FT->source_abort (FT, t, s);

	return TRUE;
}

static void push_forward_error (FTNode *node, in_addr_t ip, const char *errstr)
{
	Array  *downloads;
	size_t  i, cnt;
	int     n;

	/* print a happy little message to the user informing them that something
	 * went awry */
	FT->DBGSOCK (FT, FT_CONN(node), "err: %s: %s",
	             net_ip_str (ip), STRING_NOTNULL(errstr));

	/*
	 * Access all downstream FTTransfer objects, and nuke all the firewalled
	 * sources that match the remote peer as their search parent and are from
	 * the named user
	 */
	if (!(downloads = ft_downloads_access ()))
	{
		FT->DBGFN (FT, "hmm, no local sources found?");
		return;
	}

	n = 0;

	for (i = 0, cnt = array_count (&downloads); i < cnt; i++)
		n += nuke_source (node, ip, array_index (&downloads, i));

	array_unset (&downloads);

	FT->DBGFN (FT, "removed %i sources", n);
}

static void push_forward_success (FTNode *node, in_addr_t ip)
{
	/* remove the push forward purpose and drop the connection if there's
	 * no longer a reason to maintain it */
	ft_session_drop_purpose (node, FT_PURPOSE_PUSH_FWD);
}

FT_HANDLER (ft_push_fwd_response)
{
	in_addr_t   ip;
	uint16_t    success = FALSE;
	char       *errstr;

	ip = ft_packet_get_ip (packet);

	/* pre-0.1.0.6, this command was always an error, it is now being used
	 * for both success and error conditions */
	success = ft_packet_get_uint16 (packet, TRUE);

	errstr = ft_packet_get_str (packet);

	if (success)
		push_forward_success (FT_NODE(c), ip);
	else
		push_forward_error (FT_NODE(c), ip, errstr);
}
