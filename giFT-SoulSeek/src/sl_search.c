/*
 * Copyright (C) 2003 Arend van Beelen jr. (arend@auton.nl)
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

#include "sl_soulseek.h"
#include "sl_search.h"

/*****************************************************************************/

static const int MAX_SEARCHES = 128;
static const int MAX_BROWSES = 64;

static SLSearchList *searches = NULL;
static int num_searches;
static SLBrowseList *browses = NULL;
static int num_browses;
static uint32_t current_token;

/*****************************************************************************/

// initialize the searches
SLSearchList *sl_search_create()
{
	searches = MALLOC(MAX_SEARCHES * sizeof(SLSearchList));
	num_searches = 0;

	browses = MALLOC(MAX_BROWSES * sizeof(SLBrowseList));
	num_browses = 0;

	current_token = rand() % 4096; // this is to reduce the chance we will start with a token
	                               // we used in a previous session

	return searches;
}

// returns a search list item that has the given token associated
SLSearchList *sl_search_get_search(uint32_t token)
{
	assert(searches != NULL);

	int i;

	for(i = 0; i < num_searches; i++)
	{
		if(searches[i].token == token)
			return &searches[i];
	}

	return NULL;
}

// returns a browse list item for the given username
SLBrowseList *sl_search_get_browse(char *username)
{
	assert(browses != NULL);

	int i;

	for(i = 0; i < num_browses; i++)
	{
		if(strcmp(browses[i].username, username) == 0)
			return &browses[i];
	}

	return NULL;
}

// returns a browse list item for the given peer
SLBrowseList *sl_search_get_browse_by_peer(SLPeer *peer)
{
	assert(browses != NULL);

	int i;

	for(i = 0; i < num_browses; i++)
	{
		if(browses[i].peer == peer)
			return &browses[i];
	}

	return NULL;
}

// cleans up the search list
void sl_search_destroy(SLSearchList *searches)
{
	int i;
	for(i = 0; i < num_searches; i++)
	{
		if(searches[i].exclude != NULL)
			free(searches[i].exclude);
	}
	free(searches);
}

// called by giFT to initiate search
int sl_gift_cb_search(Protocol *p, IFEvent *event, char *query, char *exclude, char *realm, Dataset *meta)
{
	if(SL_SESSION->tcp_conn == NULL || num_searches == MAX_SEARCHES)
		return FALSE;

	// put the search in the search list
	searches[num_searches].token = current_token;
	searches[num_searches].p = p;
	searches[num_searches].event = event;
	if(exclude != NULL)
	{
		searches[num_searches].exclude = MALLOC(strlen(exclude) + 1);
		strcpy(searches[num_searches].exclude, exclude);
	}
	else
		searches[num_searches].exclude = NULL;

	// send the search request to the server
	SLPacket *packet = sl_packet_create();
	sl_packet_set_type(packet, SLFileSearch);
	sl_packet_insert_integer(packet, current_token);
	sl_packet_insert_c_string(packet, query);
	sl_packet_send(SL_SESSION->tcp_conn, packet);
	sl_packet_destroy(packet);

	current_token++;
	num_searches++;

	return TRUE;
}

static void send_browse_req(TCPC *peer_conn, void *udata)
{
	sl_peer_conn_send_get_shared_file_list(peer_conn);
}

static void conn_req_failed(void *udata)
{
	SL_PROTO->dbg(SL_PROTO, "Browse failed.");
}

// called by giFT to initiate browse
int sl_gift_cb_browse(Protocol *p, IFEvent *event, char *user, char *node)
{
	SLPeer    *peer;
	sl_string *username = sl_string_create_with_contents(user);

	if((peer = sl_find_peer(username)) == NULL)
		peer = sl_peer_new(username);
	
	sl_peer_req_conn(peer, SLConnPeer, send_browse_req, conn_req_failed, NULL);
	
	return TRUE;
}

// called by giFT to locate file
int sl_gift_cb_locate(Protocol *p, IFEvent *event, char *htype, char *hash)
{
	return FALSE;
}

// called by giFT to cancel search/locate/browse
void sl_gift_cb_search_cancel(Protocol *p, IFEvent *event)
{
	int i;

	// look up the specified search in the list ...
	for(i = 0; i < num_searches; i++)
	{
		if(searches[i].p == p && searches[i].event == event)
		{
			// ... and delete it when found
			if(searches[i].exclude != NULL)
				free(searches[i].exclude);

			num_searches--;
			searches[i].token   = searches[num_searches].token;
			searches[i].p       = searches[num_searches].p;
			searches[i].event   = searches[num_searches].event;
			searches[i].exclude = searches[num_searches].exclude;

			return;
		}
	}
}

/*****************************************************************************/
