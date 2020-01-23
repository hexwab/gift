/*
 * $Id: ft_search.c,v 1.60 2004/07/11 00:16:14 jasta Exp $
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

#include "ft_http.h"
#include "ft_search.h"
#include "ft_search_obj.h"
#include "ft_search_exec.h"

#include "ft_netorg.h"

/*****************************************************************************/

/**
 * Simple interface structure to simplify the internal implementations for
 * ::ft_search_reply, ::ft_browse_reply, and ::ft_search_reply_self.
 */
struct search_result
{
	Share         *file;               /**< Record this result refers to */

	ft_nodeinfo_t *owner;              /**< Publisher of this result */
	unsigned int   avail;              /**< Open positions on owner's queue */

	ft_nodeinfo_t *parent;
};

/*****************************************************************************/

/*
 * Build the OpenFT-specific URL field.  Keep in mind that the giFT
 * protocol URLs are not actually required to comply with any standard
 * syntax rules for a URL.  In retrospect I feel that this decision was a
 * huge mistake but it's difficult to go back now.
 */
static char *build_openft_url (struct search_result *result)
{
	String *urlbuf;
	char *pathenc;

	urlbuf = string_new (NULL, 0, 0, TRUE);
	assert (urlbuf != NULL);

	/*
	 * I've recently had a change of heart regarding compliance with some
	 * standards regarding additional URL schemes.  Let's try for something
	 * much cleaner, much more abstract, and much more compliant.
	 */
#if 0 /* RFC1738_COMPLIANT */

# error FINISHME

#else /* !RFC1738_COMPLIANT */

	/* begin with the scheme and host */
	string_append (urlbuf, "OpenFT://");
	string_append (urlbuf, net_ip_str (result->owner->host));

	/* the result publisher is unavailable for direct contact, so lets add
	 * forwarding information */
	if (result->owner->indirect)
	{
		/* this is ugly syntax, and will be phased out over time */
		string_appendf (urlbuf, ":%hu@",
		                (unsigned short)(openft->ninfo.port_http));

		string_append  (urlbuf, net_ip_str (result->parent->host));
		string_appendf (urlbuf, ":%hu",
		                (unsigned short)(result->parent->port_openft));
	}
	else
	{
		/* add the direct owners HTTP port explicitly */
		string_appendf (urlbuf, ":%hu",
		                (unsigned short)(result->owner->port_http));
	}

	/* append the urlencoded share path including the leading forward
	 * slash character */
	if (!(pathenc = http_url_encode (result->file->path)))
	{
		string_free (urlbuf);
		return NULL;
	}

	string_append (urlbuf, pathenc);
	free (pathenc);

#endif /* RFC1738_COMPLIANT */

	return string_free_keep (urlbuf);
}

static BOOL deliver_result (IFEvent *event, struct search_result *result)
{
	char *urlstr;
	char *ownername;
	char *parentname;

	/*
	 * Construct the openft:// specific URL that is used to communicate all
	 * the necessary resources for locating and downloading an OpenFT share
	 * on another peer.  This works for indirect download sources, such
	 * as those users who have configured with port=0.
	 */
	if (!(urlstr = build_openft_url (result)))
	{
		FT->DBGFN (FT, "REPORTME: failed to build openft:// url");
		return FALSE;
	}

	/*
	 * Access the formal OpenFT name for the owner and parent that returned
	 * the search result, which will include node alias information if its
	 * available.  Please forgive the strdup calls as they are necessary
	 * because of the usage of a static buffer in net_ip_str (*sigh*).
	 */
	ownername  = STRDUP (ft_node_user_host (result->owner->host,
	                                        result->owner->alias));
	parentname = STRDUP (ft_node_user_host (result->parent->host,
	                                        result->parent->alias));

	assert (ownername != NULL);
	assert (parentname != NULL);

	/* finally, actually send the search/browse result to giFT */
	FT->search_result (FT, event, ownername, parentname, urlstr,
	                   result->avail, result->file);

	free (ownername);
	free (parentname);
	free (urlstr);

	return TRUE;
}

/*****************************************************************************/

static BOOL search_reply_term (FTSearch *srch, struct search_result *result)
{
	/*
	 * Notify the search object that we have received a complete reply from
	 * the named host.  We will then be given the number of hosts we are left
	 * waiting on.  If 0, we will be resposnible for tidying up the search
	 * object.
	 */
	if (ft_search_rcvdfrom (srch, result->parent->host) == 0)
		ft_search_finish (srch);

	/* uhh, what defines failure for us? */
	return TRUE;
}

static BOOL search_reply (FTSearch *srch, struct search_result *result)
{
	ft_search_parms_t *params;

	/* just to catch any funky usage that might arise */
	assert (result->owner != NULL);

	/* shorthand */
	params = &srch->params;

	/* we are firewalled, they are firewalled...drop the result */
	if (openft->ninfo.indirect && result->owner->indirect)
		return FALSE;

	/*
	 * Verify the search result locally.  That is, make sure that if we were
	 * configured as a search node and executed a search with the same
	 * parameters we would have concluded that this was in fact a valid
	 * search result.
	 */
	if (FT_CFG_SEARCH_VFY)
	{
		BOOL scmp;
		ft_search_flags_t flags;

		/* make sure we negate the "hidden" flag from the search that we sent
		 * out to remote search peers to convince ::ft_search_cmp that we are
		 * in fact dealing with the original search query string */
		flags = params->type & ~FT_SEARCH_HIDDEN;

		scmp = ft_search_cmp (result->file, flags,
		                      params->realm, params->query, params->exclude);

		/* search did NOT match */
		if (!scmp)
		{
			/* only be noisy if the user wants us to, as this could
			 * potentially get very loud, especially if the search node is
			 * intentionally trying to deceive users */
			if (FT_CFG_SEARCH_VFY_NOISY)
			{
				FT->DBGFN (FT, "%p('%s'): %s: verification failed!",
				           srch, params->query, result->file->path);
			}

			/* do not return this result to the user */
			return FALSE;
		}
	}

	/* go ahead and send the search to giFT */
	return deliver_result (srch->event, result);
}

static BOOL search_reply_logic (FTSearch *srch, struct search_result *result)
{
	BOOL ret;

	assert (result != NULL);

	/* handle the special end-of-search exception which admittedly should've
	 * been implemented by a separate function entirely */
	if (!result->file)
		ret = search_reply_term (srch, result);
	else
		ret = search_reply (srch, result);

	return ret;
}

BOOL ft_search_reply (FTSearch *srch,
                      ft_nodeinfo_t *owner, ft_nodeinfo_t *parent,
                      Share *share, unsigned int avail)
{
	static struct search_result result;

	assert (srch != NULL);

	result.file = share;
	result.owner = owner;
	result.avail = avail;
	result.parent = parent;

	return search_reply_logic (srch, &result);
}

BOOL ft_search_reply_self (FTSearch *srch, ft_nodeinfo_t *owner,
                           Share *share, unsigned int avail)
{
	return ft_search_reply (srch, owner, &openft->ninfo, share, avail);
}

BOOL ft_browse_reply (FTBrowse *browse, ft_nodeinfo_t *owner, Share *share,
                      unsigned int avail)
{
	static struct search_result result;

	assert (browse != NULL);
	assert (share != NULL);

	result.file = share;
	result.owner = owner;
	result.avail = avail;
	result.parent = &openft->ninfo;

	/*
	 * I have a very strong suspicion that we are not explicitly terminating
	 * the browse as we do when share=NULL in ft_search_reply.  There is a
	 * 4 minute timer running from ft_search_obj.c that will guarantee
	 * eventual termination, but I can't seem to confirm that hte browse
	 * is ever explicitly terminated.  Sigh.
	 */
	return deliver_result (browse->event, &result);
}

/*****************************************************************************/

static uint16_t get_search_ttl (int type)
{
	uint16_t ttl = FT_CFG_SEARCH_TTL;

	if (FT_SEARCH_METHOD(type) == FT_SEARCH_MD5)
		ttl++;

	if (openft->ninfo.klass & FT_NODE_SEARCH)
		ttl--;

	return ttl;
}

static BOOL send_search (FTNode *node, FTSearch *search)
{
	FTPacket *pkt;
	int       type;

	assert (search != NULL);
	assert (search->event != NULL);

	/* use only search nodes that have a fully handshaked connection to us */
	if (node->session->stage < 4)
		return FALSE;

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
	 *
	 * We now add additional port information (will be filled in by the
	 * remote peer once received) so that the remote node(s) receiving this
	 * search may filter results for us based on our firewalled status with
	 * them.
	 */
	ft_packet_put_ip (pkt, 0);

	if (node->version >= OPENFT_0_2_0_1)
		ft_packet_put_uint16 (pkt, 0, TRUE);

	/* add the TTL of this search, that is, the number of nodes that will
	 * forward this search along to their peers until the search is stopped
	 * in the network */
	ft_packet_put_uint16 (pkt, get_search_ttl (type), TRUE);

	/* write the maximum number of results we want to get back, so that this
	 * search may more efficiently complete when the result set is large */
	ft_packet_put_uint16 (pkt, FT_CFG_SEARCH_RESULTS_REQ, TRUE);

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

	ft_search_sentto (search, node->ninfo.host);
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
		FTNode *node = ft_share->node;
		BOOL indirect;

		assert (node != NULL);

		/* hack to forced verified=false to translate to indirect=true */
		indirect = node->ninfo.indirect;

		if (node->session->verified == FALSE)
			node->ninfo.indirect = TRUE;

		ft_search_reply_self (search, &node->ninfo, share,
		                      node->session->avail);

		node->ninfo.indirect = indirect;
	}

	ft_share_unref (share);

	return ret;
}

static void search_children (FTSearch *search)
{
	ft_search_parms_t *params;

	params = &search->params;
	ft_search (0, (FTSearchResultFn)result_reply, search,
	           params->type, params->realm, params->query, params->exclude);
}

static int search_parents (FTSearch *search)
{
	int n;

	if (openft->ninfo.klass & FT_NODE_SEARCH)
	{
		n = ft_netorg_foreach (FT_NODE_SEARCH, FT_NODE_CONNECTED, FT_CFG_SEARCH_MAXPEERS,
		                       FT_NETORG_FOREACH(send_search), search);
	}
	else
	{
		n = ft_netorg_foreach (FT_NODE_PARENT, FT_NODE_CONNECTED, FT_CFG_SEARCH_PARENTS,
				       FT_NETORG_FOREACH(send_search), search);
	}
	else
	{
		n = ft_netorg_foreach (FT_NODE_PARENT, FT_NODE_CONNECTED, FT_CFG_SEARCH_PARENTS,
				       FT_NETORG_FOREACH(send_search), search);
	}

	FT->DBGFN (FT, "%s: searched %i nodes (ttl=%u)",
	           ft_guid_fmt (search->guid), n,
	           (unsigned int)(get_search_ttl (search->params.type)));

	/* we are using this logic to determine how many parents are being waited
	 * on, so that the code can clean itself up immediately if no nodes were
	 * found */
	if (search->waiting_on == NULL)
		assert (n == 0);

	return n;
}

/*****************************************************************************/

static int exec_search (Protocol *p, IFEvent *event, ft_search_flags_t type,
                        char *query, char *exclude, char *realm, Dataset *meta)
{
	FTSearch *search;

	if (!(search = ft_search_new (event, type, realm, query, exclude)))
		return FALSE;

	/*
	 * Search all of our children's shares as if a remote node executed the
	 * search.
	 */
	if (openft->ninfo.klass & FT_NODE_SEARCH)
		search_children (search);

	/*
	 * This searches all search nodes you are connected to acting as your
	 * parent as well as your search peers if you yourself are a search node
	 */
	if (search_parents (search) == 0)
	{
		/* clean up the search and tell giFT that we have failed to execute
		 * the search, this will call if_search_remove directly in giFT
		 * space */
		ft_search_disable (search);
		ft_search_finish (search);
		return FALSE;
	}

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
	/* We will only find MD5 hashes. */
	if (STRCMP (htype, "MD5"))
		return FALSE;

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
