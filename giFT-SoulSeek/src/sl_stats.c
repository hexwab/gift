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
#include "sl_stats.h"

/*****************************************************************************/

// initialize stats
SLStats *sl_stats_create()
{
	SLStats *stats = MALLOC(sizeof(SLStats));

	stats->users = 0;
	stats->files = 0;
	stats->size = 0;
	stats->numrooms = 0;
	stats->rooms = NULL;

	return stats;
}

// destroy stats
void sl_stats_destroy(SLStats *stats)
{
	assert(stats != 0);

	unsigned int i;
	if(stats->numrooms > 0)
	{
		for(i = 0; i < stats->numrooms; i++)
		{
			free(stats->rooms[i].name);
		}
		free(stats->rooms);
	}

	free(stats);
}

// giFT callback to retrieve stats
int sl_gift_cb_stats(Protocol *p, unsigned long *users, unsigned long *files, double *size, Dataset **extra)
{
	*users = SL_PLUGIN->stats->users;
	*files = SL_PLUGIN->stats->files;
	*size  = SL_PLUGIN->stats->size;

	return 1; // number of connections
}

// receives a new roomlist
void sl_stats_receive_roomlist(SLStats *stats, uint8_t *data)
{
	SL_PROTO->dbg(SL_PROTO, "Received RoomList packet.");

	SLPacket *packet;
	uint32_t numrooms;
	sl_string *string;
	unsigned int i;
	int error;

	if((packet = sl_packet_create_from_data(data)) == NULL)
		return;

	numrooms = sl_packet_get_integer(packet, &error);
	if(error)
	{
		SL_PROTO->warn(SL_PROTO, "Couldn't get number of rooms.");
		return;
	}
	SL_PROTO->dbg(SL_PROTO, "Number of rooms: %d", numrooms);
	stats->rooms = MALLOC(numrooms * sizeof(SLRoom));
	stats->numrooms = numrooms;
	for(i = 0; i < numrooms; i++)
	{
		if((string = sl_packet_get_string(packet)) == NULL)
		{
			SL_PROTO->warn(SL_PROTO, "Couldn't get name of room %d.", i);
			stats->rooms[i].name = NULL;
			continue;
		}
		stats->rooms[i].name = MALLOC(string->length + 1);
		strcpy(stats->rooms[i].name, string->contents);
	}
	numrooms = sl_packet_get_integer(packet, &error);
	if(error)
	{
		SL_PROTO->warn(SL_PROTO, "Couldn't get number of room populations.");
		return;
	}
	if(numrooms != stats->numrooms)
	{
		SL_PROTO->dbg(SL_PROTO, "Second number of rooms differs, strange.");
	}
	stats->users = 0;
	for(i = 0; i < numrooms && i < stats->numrooms; i++)
	{
		stats->rooms[i].users = sl_packet_get_integer(packet, &error);
		if(error)
			break;
		stats->users += stats->rooms[i].users;
	}
	for( ; i < stats->numrooms; i++)
	{
		stats->rooms[i].users = 0;
	}

	SL_PROTO->dbg(SL_PROTO, "RoomList packet OK.");
}

// sends a request for the global userlist
void sl_stats_send_global_userlist_request(SLSession *session)
{
	// create the packet
	SLPacket *packet = sl_packet_create();
	sl_packet_set_type(packet, SLGlobalUserList);

	// send the packet
	sl_packet_send(session->tcp_conn, packet);

	sl_packet_destroy(packet);
}

// receives the global userlist
void sl_stats_receive_global_userlist(SLStats *stats, uint8_t *data)
{
	SL_PROTO->dbg(SL_PROTO, "Received GlobalUserList packet...");

	SLPacket *packet;
	uint32_t users;
	int error;

	if((packet = sl_packet_create_from_data(data)) == NULL)
		return;

	users = sl_packet_get_integer(packet, &error);
	if(error)
	{
		SL_PROTO->warn(SL_PROTO, "Couldn't get number of users.");
		return;
	}
	SL_PROTO->dbg(SL_PROTO, "Number of users: %d", users);
	stats->users = users;

	SL_PROTO->dbg(SL_PROTO, "GlobalUserList packet OK.");
}

/*****************************************************************************/
