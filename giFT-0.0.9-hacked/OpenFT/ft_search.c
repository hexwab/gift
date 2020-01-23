/*
 * $Id: ft_search.c,v 1.40 2003/06/01 07:26:13 jasta Exp $
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

#include "ft_html.h"
#include "ft_search.h"
#include "ft_search_obj.h"
#include "ft_search_exec.h"

#include "ft_netorg.h"

/*****************************************************************************/

/* send a result back to the interface protocol (through giFT) */
static void send_result (IFEvent *event, FTNode *owner, FTNode *parent,
                         Share *share, unsigned int avail)
{
	String *s;
	char   *pathenc;
	char   *user;
	char   *node;

	/* make sure a cancelled search didnt make its way up here... */
	assert (event != NULL);

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return;

	/* begin with the common url construction */
	string_appendf (s, "OpenFT://%s", net_ip_str (owner->ip));

	/* if there was a parent search node provided, and the result is
	 * considered to be "firewalled", then construct a special result which
	 * can be used to issue a push request */
	if (parent && ft_node_fw (owner))
	{
		/* construct a special firewalled URL that only OpenFT can
		 * understand */
		string_appendf (s, ":%hu@%s:%hu",
		                (unsigned short)FT_SELF->http_port, net_ip_str (parent->ip),
		                (unsigned short)parent->port);
	}
	else
	{
		/* append the simple HTTP port */
		string_appendf (s, ":%hu", (unsigned short)owner->http_port);
	}

	if (!(pathenc = url_encode (share->path)))
	{
		string_free (s);
		return;
	}

	string_append (s, pathenc);
	free (pathenc);

	/* we have to duplicate here because they both try to use the same
	 * internal net_ip_str buffer */
	user = STRDUP (ft_node_user (owner));
	node = STRDUP (ft_node_user (parent));

	FT->search_result (FT, event, user, node, s->str, avail, share);

	free (user);
	free (node);

	string_free (s);
}

void ft_search_reply (FTSearch *srch, FTNode *owner, FTNode *parent,
                      Share *share, unsigned int avail)
{
	FTSearchParams *params;

	if (!srch)
		return;

	/* end-of-search exception */
	if (!share)
	{
		/* if we have no more parents being waited on, finish the search
		 * right here...kinda like a reference counting system */
		if (ft_search_rcvdfrom (srch, parent) == 0)
			ft_search_finish (srch);

		return;
	}

	/* make sure we are getting what we expect here, as this interface has
	 * gone through many changes */
	params = &srch->params;
	assert (owner != NULL);
	assert (parent != NULL);

#if 0
	if (!(ft_share = share_get_udata (share, "OpenFT")))
	{
		FT->DBGFN (FT, "cannot lookup OpenFT-specific share data!");
		return;
	}
#endif

    /* the search result is firewalled as is this local node, displaying the
	 * result will simply be annoying */
	if (ft_node_fw (owner) && ft_node_fw (FT_SELF))
		return;

	/* verify that this wasnt a faulty search result, that is, make sure if
	 * we had performed the search locally we would have found this to be a
	 * solid match */
	if (!ft_search_cmp (share, params->type & ~FT_SEARCH_HIDDEN,
	                    params->realm, params->query, params->exclude))
	{
		/* failed match, silently ignore this result... */
		return;
	}

	/* normal search result, build the giFT-specific parts of the reply and
	 * send away... */
	send_result (srch->event, owner, parent, share, avail);
}

void ft_browse_reply (FTBrowse *browse, FTNode *owner, Share *share,
                      unsigned int avail)
{
	if (!browse)
		return;

	send_result (browse->event, owner, NULL, share, avail);
}

/*****************************************************************************/

static int send_search (FTNode *node, FTSearch *search)
{
	FTPacket *pkt;
	int       type;

	assert (search != NULL);
	assert (search->event != NULL);

	if (node->version < OPENFT_0_0_9_6)
	{
		FT->DBGSOCK (FT, FT_CONN(node), "refusing to send search (%08x)",
		             node->version);
		return FALSE;
	}

	type = search->params.type;

#ifdef FT_SEARCH_PARANOID
	if (FT_SEARCH_TYPE(type) == FT_SEARCH_FILENAME)
		type |= FT_SEARCH_HIDDEN;
#endif /* FT_SEARCH_PARANOID */

	if (!(pkt = ft_packet_new (FT_SEARCH_REQUEST, 0)))
		return FALSE;

	/* add the search event id, this will be unique to this OpenFT connection
	 * until the search completes */
	ft_packet_put_ustr (pkt, search->guid, FT_GUID_SIZE);

	/*
	 * As a courtesy/optimization, the _ORIGINAL_ source of the search will
	 * be recorded when forwarded along so that search nodes can avoid
	 * bouncing back there if it happens to be a peer somewhere down the
	 * line.  We will use 0 here (being the originator) so that the peers
	 * we deliver to will fill in the "blank" when they forward along.
	 */
	ft_packet_put_ip (pkt, 0);

	/* add the TTL of this search, that is, the number of nodes that will
	 * forward this search along to their peers until the search is stopped
	 * in the network */
	ft_packet_put_uint16 (pkt, FT_SEARCH_TTL, TRUE);

	/* write the maximum number of results we want to get back, so that this
	 * search may more efficiently complete when the result set is large */
	ft_packet_put_uint16 (pkt, FT_SEARCH_RESULTS_REQ, TRUE);

	/* pass along the exact operation we wish performed, OR'd with any
	 * options we might have (such as FT_SEARCH_HIDDEN) */
	ft_packet_put_uint16 (pkt, type, TRUE);

	/* add the actual query data */
#ifdef FT_SEARCH_PARANOID
	ft_packet_put_uarray (pkt, 4, search->params.qtokens, TRUE);
	ft_packet_put_uarray (pkt, 4, search->params.etokens, TRUE);
#else
	ft_packet_put_str (pkt, search->params.query);
	ft_packet_put_str (pkt, search->params.exclude);
#endif /* FT_SEARCH_PARANOID */

	ft_packet_put_str (pkt, search->params.realm);

	/* off you go */
	if (ft_packet_send (FT_CONN(node), pkt) < 0)
		return FALSE;

#if 0
	FT->DBGSOCK (FT, FT_CONN(node), "%s", ft_guid_fmt (search->guid));
#endif

	ft_search_sentto (search, node);
	return TRUE;
}

static BOOL result_reply_hack (FTSearch *search, Share *share, FTSHost *shost)
{
	FTNode owner;

	/* the shost interface is deprecated and will be going away soon, so
	 * we will only wrap it here for now */
	owner.ip        = shost->host;
	owner.port      = shost->ft_port;
	owner.http_port = shost->http_port;
	owner.alias     = shost->alias;

	/* pray this was enough to satisfy the interface */
	ft_search_reply (search, &owner, FT_SELF, share, shost->availability);

	return TRUE;
}

static BOOL result_reply (Share *share, FTSearch *search)
{
	FTShare *ft_share;
	BOOL     ret = FALSE;

	if (!share)
		return TRUE;

	if ((ft_share = share_get_udata (share, "OpenFT")))
	{
		assert (ft_share->shost != NULL);

		/* wrap the new ft_search_reply interface so that it will work
		 * with the deprecated FTSHost objects for now... */
		ret = result_reply_hack (search, share, ft_share->shost);
	}

	ft_share_unref (share);

	return ret;
}

static void search_children (FTSearch *search)
{
	FTSearchParams *params;

	params = &search->params;
	ft_search (0, (FTSearchResultFn)result_reply, search,
	           params->type, params->realm, params->query, params->exclude);
}

static int search_parents (FTSearch *search)
{
	int n;

	n = ft_netorg_foreach (FT_NODE_PARENT, FT_NODE_CONNECTED, FT_SEARCH_PARENTS,
	                       FT_NETORG_FOREACH(send_search), search);

	if (FT_SELF->klass & FT_NODE_SEARCH)
	{
		n += ft_netorg_foreach (FT_NODE_SEARCH, FT_NODE_CONNECTED, FT_SEARCH_PEERS,
		                        FT_NETORG_FOREACH(send_search), search);
	}

	FT->DBGFN (FT, "%s: searched %i nodes (ttl=%i)",
	           ft_guid_fmt (search->guid), n, FT_SEARCH_TTL);

	/* we are using this logic to determine how many parents are being waited
	 * on, so that the code can clean itself up immediately if no nodes were
	 * found */
	if (!search->waiting_on)
		assert (n == 0);

	return n;
}

/*****************************************************************************/

static int exec_search (Protocol *p, IFEvent *event, FTSearchType type,
                        char *query, char *exclude, char *realm, Dataset *meta)
{
	FTSearch *search;

	if (!(search = ft_search_new (event, type, realm, query, exclude)))
		return FALSE;

	/*
	 * Search all of our children's shares as if a remote node executed the
	 * search.
	 */
	if (FT_SELF->klass & FT_NODE_SEARCH)
		search_children (search);

	/*
	 * This searches all search nodes you are connected to ... obviously this
	 * is not a blocking operation.  The rest of this code moves over to
	 * ft_protocol.c:ft_search_response.
	 */
	if (search_parents (search) == 0)
		return FALSE;

	assert (search->waiting_on != NULL);

	/*
	 * Let giFT know everything went ok and that it can expect replies on
	 * this event in a short while.
	 */
	return TRUE;
}

int openft_search (Protocol *p, IFEvent *event, char *query, char *exclude,
                   char *realm, Dataset *meta)
{
	return exec_search (p, event, FT_SEARCH_FILENAME, query, exclude, realm,
	                    meta);
}

static int exec_browse (Protocol *p, IFEvent *event, in_addr_t user)
{
	FTPacket *packet;
	FTBrowse *browse;

	if (!(packet = ft_packet_new (FT_BROWSE_REQUEST, 0)))
		return FALSE;

	if (!(browse = ft_browse_new (event, user)))
	{
		ft_packet_free (packet);
		return FALSE;
	}

	ft_packet_put_ustr (packet, browse->guid, FT_GUID_SIZE);

	/*
	 * We are using sendto here in the hopes that it will eventually
	 * provide abstraction for establishing a connection, and scheduling
	 * our packet for delivery once handshaked.  For now, it does not do that.
	 */
	if (ft_packet_sendto (user, packet) < 0)
	{
		/*
		 * We are unable to deliver this message for whatever reason,
		 * abort the OpenFT search object and inform giFT that we
		 * failed.
		 */
		FT->DBGFN (FT, "browse failed...sigh");
		ft_browse_finish (browse);
		return FALSE;
	}

	return TRUE;
}

int openft_browse (Protocol *p, IFEvent *event, char *user, char *node)
{
	char       *ptr;
#if 0
	FTNode     *user_node;
#endif

	FT->DBGFN (FT, "browsing %s", user);

	/*
	 * It's possible that this browse was executed w/ the node alias, in
	 * which case we want to hack it off as it is utterly worthless in
	 * tracking down this user.
	 */
	if ((ptr = strchr (user, '@')))
		user = ptr + 1;

#if 0
	/*
		* Attempt to lookup the appropriate (already established) connection
		* to this user.  If none can be found, we should attempt to gather one,
		* however this code is not that sophisticated yet.  So we gracefully
		* fail and warn the user that this function is not complete.
		*/
	if (!(user_node = ft_netorg_lookup (net_ip (user))))
	{
		TRACE (("TODO: not connected to %s", user));
		return FALSE;
	}
#endif

	/*
	 * Actually send the browsing packets to OpenFT.
	 */
	return exec_browse (p, event, net_ip (user));
}

int openft_locate (Protocol *p, IFEvent *event, char *htype, char *hash)
{
	return exec_search (p, event, FT_SEARCH_MD5, hash, NULL, NULL, NULL);
}

void openft_search_cancel (Protocol *p, IFEvent *event)
{
	FTSearch *search;
	FTBrowse *browse;

	if ((search = ft_search_find_by_event (event)))
		ft_search_disable (search);
	else if ((browse = ft_browse_find_by_event (event)))
		ft_browse_disable (browse);
}
