/*
 * ft_search.c
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

#include "ft_openft.h"

#include "ft_html.h"
#include "ft_search.h"
#include "ft_search_exec.h"

#include "ft_netorg.h"

/*****************************************************************************/

static Dataset *searches = NULL;

/*****************************************************************************/

static int add_search (IFEvent *event, FTSearch *search)
{
	IFEventID id;

	if (!event)
		return FALSE;

	/* create the giFT <-> OpenFT event link
	 * NOTE: this relationship becomes recursive by ft_event_new's
	 * calling */
	if (!(search->event = ft_event_new (event, event->id, "FTSearch", search)))
		return FALSE;

	id = search->event->id;

	/* make sure we dont lose track of searches */
	assert (dataset_lookup (searches, &id, sizeof (id)) == NULL);
	dataset_insert (&searches, &id, sizeof (id), search, 0);

	return TRUE;
}

FTSearch *ft_search_new (IFEvent *event, FTSearchType type,
                         char *query, char *exclude, char *realm)
{
	FTSearch *search;

	if (!(search = MALLOC (sizeof (FTSearch))))
		return NULL;

	if (!add_search (event, search))
	{
		free (search);
		return NULL;
	}

	search->type    = type;
	search->query   = STRDUP (query);
	search->exclude = STRDUP (exclude);
	search->realm   = STRDUP (realm);

	search->qtokens = ft_search_tokenize (search->query);
	search->etokens = ft_search_tokenize (search->exclude);

	return search;
}

static void finish_search (FTSearch *search)
{
	assert (search->event != NULL);

	/* inform giFT that the OpenFT protocol has returned all there will be
	 * for this search */
	if (search->event->active)
		openft_p->search_complete (openft_p, search->event->event);

	/* delete the association */
	ft_event_free (search->event);
}

static void ft_search_free (FTSearch *search)
{
	if (!search)
		return;

	finish_search (search);

	dataset_clear (search->ref);

	free (search->query);
	free (search->exclude);
	free (search->realm);

	free (search->qtokens);
	free (search->etokens);

	free (search);
}

static void ft_search_remove (FTSearch *search)
{
	dataset_remove (searches, &search->event->id, sizeof (search->event->id));
	ft_search_free (search);
}

/*****************************************************************************/

static void add_parent (FTSearch *search, in_addr_t parent)
{
	if (!search->ref)
		search->ref = dataset_new (DATASET_LIST);

	dataset_insert (&search->ref, &parent, sizeof (parent), "in_addr_t", 0);
}

static unsigned long remove_parent (FTSearch *search, in_addr_t parent)
{
	dataset_remove (search->ref, &parent, sizeof (parent));
	return dataset_length (search->ref);
}

/*****************************************************************************/

void ft_search_need_reply (FTSearch *search, in_addr_t host)
{
	if (!search)
		return;

	add_parent (search, host);
}

/*****************************************************************************/

static void send_result (FTSearch *search, Connection *snode,
                         FileShare *file, FTShare *share)
{
	String *s;
	char   *pathenc;
	char   *user;
	char   *node;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return;

	assert (share->shost->verified == TRUE);

	/* begin with the common url construction */
	string_appendf (s, "OpenFT://%s", net_ip_str (share->shost->host));

	if (snode && (share->shost->ft_port == 0 || !share->shost->verified))
	{
		/* construct a special firewalled URL that only OpenFT can
		 * understand */
		string_appendf (s, ":%hu@%s:%hu",
		                FT_SELF->http_port, net_ip_str (FT_NODE(snode)->ip),
		                FT_NODE(snode)->http_port);
	}
	else
	{
		/* append the simple HTTP port */
		string_appendf (s, ":%hu", share->shost->http_port);
	}

	if (!(pathenc = url_encode (SHARE_DATA(file)->path)))
	{
		string_free (s);
		return;
	}

	string_append (s, pathenc);
	free (pathenc);

	user = STRDUP (ft_node_user_host (share->shost->host, share->shost->alias));
	node = STRDUP (ft_node_user (FT_NODE(snode)));

#if 0
	TRACE (("%s (%s): %s", user, node, s->str));
#endif

	openft_p->search_result (openft_p, search->event->event,
	                        user, node, s->str, share->shost->availability,
	                        file);

	free (user);
	free (node);

	string_free (s);
}

/* OpenFT -> daemon -> user interface search result */
void ft_search_reply (FTSearch *search, Connection *snode, FileShare *file)
{
	FTShare *share;

	if (!search)
		return;

	assert (snode != NULL);

	/* end of search */
	if (!file)
	{
		if (!remove_parent (search, FT_NODE(snode)->ip))
			ft_search_remove (search);

		return;
	}

	if (!(share = share_lookup_data (file, "OpenFT")))
		return;

	assert (share->shost != NULL);

	/* the search result is firewalled as is this local node, displaying the
	 * result is futile and annoying */
	if (share->shost->ft_port == 0 && FT_SELF->port == 0)
		return;

	/* verify that this wasnt a faulty search */
	if (!ft_search_cmp (file, search->type & ~FT_SEARCH_HIDDEN, search->realm,
	                    search->query, search->exclude))
	{
		/* failed match, ignore this result */
		return;
	}

	assert (search->event != NULL);
	assert (search->event->event != NULL);

	/* otherwise, this is a normal search reply, send it back to the
	 * interface protocol */
	send_result (search, snode, file, share);
}

/*****************************************************************************/

static int force_reply (Dataset *d, DatasetNode *node, in_addr_t *host)
{
	FTSearch *search = node->value;

	/* no more parents left, free the data and tell dataset_foreach to
	 * delete this node entry */
	if (!remove_parent (search, *host))
	{
		ft_search_free (search);
		return TRUE;
	}

	return FALSE;
}

void ft_search_force_reply (FTSearch *search, in_addr_t host)
{
	assert (search == NULL);           /* TODO */

	dataset_foreach (searches, DATASET_FOREACH (force_reply), &host);
}

/*****************************************************************************/

static int send_search (FTNode *node, FTSearch *search)
{
	int type;

	assert (search != NULL);
	assert (search->event != NULL);

	type = search->type;

#ifdef FT_SEARCH_PARANOID
	if (type & FT_SEARCH_FILENAME)
	{
		type |= FT_SEARCH_HIDDEN;

		ft_packet_sendva (FT_CONN(node), FT_SEARCH_REQUEST, 0, "lhLLs",
		                  search->event->id, type,
						  search->qtokens, search->etokens, search->realm);
	}
	else
#endif /* FT_SEARCH_PARANOID */
		ft_packet_sendva (FT_CONN(node), FT_SEARCH_REQUEST, 0, "lhsss",
		                  search->event->id, type,
						  search->query, search->exclude, search->realm);

	ft_search_need_reply (search, node->ip);
	return TRUE;
}

static int result_reply (FileShare *file, FTSearch *search)
{
	FTShare *share;

	if (!(share = share_lookup_data (file, "OpenFT")))
		return TRUE;

	assert (share->shost != NULL);

	if (share->shost->host)
		ft_search_reply (search, OPENFT->ft, file);

	/* this was done to keep track of all search results when used with
	 * a different api...oh well, senselessly wasting resources here */
	share->shost->files = list_remove (share->shost->files, file);
	share_unref (file);
	return TRUE;
}

static void search_children (FTSearch *search)
{
	List *r;

	/* fill r with search results */
	r = ft_search (NULL, search->type, search->realm,
	               search->query, search->exclude);

	/* reply and remove */
	r = list_foreach_remove (r, (ListForeachFunc) result_reply, search);
	list_free (r);
}

static void search_parents (FTSearch *search)
{
	ft_netorg_foreach (NODE_SEARCH, NODE_CONNECTED, 0,
	                   FT_NETORG_FOREACH(send_search), search);

	TRACE (("bounced to %i node(s)", dataset_length (search->ref)));
}

/*****************************************************************************/

static int exec_search (Protocol *p, IFEvent *event, FTSearchType type,
                        char *query, char *exclude, char *realm, Dataset *meta)
{
	FTSearch *search;

	if (!(search = ft_search_new (event, type, query, exclude, realm)))
		return FALSE;

	/*
	 * Search all of our children's shares as if a remote node executed the
	 * search.
	 */
	if (FT_SELF->klass & NODE_SEARCH)
		search_children (search);

	assert (search->ref == NULL);

	/*
	 * This searches all search nodes you are connected to ... obviously this
	 * is not a blocking operation.  The rest of this code moves over to
	 * ft_protocol.c:ft_search_response.
	 */
	search_parents (search);

	/*
	 * No parents found to search, this search can be considered finished
	 * right now so we will return to gift that we dont need any more time to
	 * complete the search.
	 */
	if (!search->ref)
		return FALSE;

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
	FTSearch *browse;

	if (!(packet = ft_packet_new (FT_BROWSE_REQUEST, 0)))
		return FALSE;

	if (!(browse = ft_search_new (event, FT_SEARCH_HOST, net_ip_str (user), NULL, NULL)))
	{
		ft_packet_free (packet);
		return FALSE;
	}

	ft_packet_put_uint32 (packet, browse->event->id, TRUE);

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
		TRACE (("browse failed...sigh"));
		ft_search_reply (browse, OPENFT->ft, NULL);
		return FALSE;
	}

	ft_search_need_reply (browse, user);
	return TRUE;
}

int openft_browse (Protocol *p, IFEvent *event, char *user, char *node)
{
	char       *ptr;
	FTNode     *user_node;

	TRACE (("browsing %s", user));

	/*
	 * It's possible that this browse was executed w/ the node alias, in
	 * which case we want to hack it off as it is utterly worthless in
	 * tracking down this user.
	 */
	if ((ptr = strchr (user, '@')))
		user = ptr + 1;

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

	/*
	 * Actually send the browsing packets to OpenFT.
	 */
	return exec_browse (p, event, net_ip (user));
}

int openft_locate (Protocol *p, IFEvent *event, char *hash)
{
	return exec_search (p, event, FT_SEARCH_MD5, hash, NULL, NULL, NULL);
}

void openft_search_cancel (Protocol *p, IFEvent *event)
{
	ft_event_disable (event);
}
