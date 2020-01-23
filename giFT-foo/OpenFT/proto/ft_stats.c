/*
 * $Id: ft_stats.c,v 1.2 2003/06/24 19:57:20 jasta Exp $
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

#include "ft_stats.h"

/*****************************************************************************/

/* a great many of the commands here are for index nodes only */
#define IS_INDEX (FT_SELF->klass & FT_NODE_INDEX)

/*****************************************************************************/

/*
 * Cache the last stats response we received from an index node so that all
 * node types can (specifically useful for parent search nodes) can
 * more-or-less respond to stats queries so long as they still have an index
 * node attached.
 */
static ft_stats_t last_stats = { 0, 0, 0.0 };

/*****************************************************************************/

FT_HANDLER (ft_stats_digest_add)
{
	ft_stats_t stats;
	in_addr_t  user;
	uint32_t   shares;
	uint32_t   size;

	if (!IS_INDEX)
		return;

	memset (&stats, 0, sizeof (ft_stats_t));

	user   = ft_packet_get_ip     (packet);
	shares = ft_packet_get_uint32 (packet, TRUE);
	size   = ft_packet_get_uint32 (packet, TRUE);     /* units: MB */

	stats.users  = 1;
	stats.shares = (unsigned int)shares;
	stats.size   = (double)size / 1024.0;             /* units: GB */

	/* store this stats digest locally */
	ft_stats_insert (FT_NODE(c)->ip, user, &stats);
}

FT_HANDLER (ft_stats_digest_remove)
{
	in_addr_t user;

	if (!IS_INDEX)
		return;

	user = ft_packet_get_ip (packet);

	ft_stats_remove (FT_NODE(c)->ip, user);
}

/*****************************************************************************/

FT_HANDLER (ft_stats_request)
{
	FTPacket  *pkt;
	ft_stats_t stats, *statsp = &stats;

	if (!IS_INDEX || !(ft_stats_collect (statsp)))
	{
		/* if we cant collect local states, we might as well reply with the
		 * last successful stats response we saw from a remote user */
		statsp = &last_stats;
	}

	if (!(pkt = ft_packet_new (FT_STATS_RESPONSE, 0)))
		return;

	ft_packet_put_uint32 (pkt,           statsp->users,  TRUE);
	ft_packet_put_uint32 (pkt,           statsp->shares, TRUE);
	ft_packet_put_uint32 (pkt, (uint32_t)statsp->size,   TRUE);

	ft_packet_send (c, pkt);
}

FT_HANDLER (ft_stats_response)
{
	uint32_t users;
	uint32_t shares;
	uint32_t size;                     /* units: GB */

	users  = ft_packet_get_uint32 (packet, TRUE);
	shares = ft_packet_get_uint32 (packet, TRUE);
	size   = ft_packet_get_uint32 (packet, TRUE);

	/* store the stats here for replying to other users who ask */
	last_stats.users  = (unsigned int)users;
	last_stats.shares = (unsigned int)shares;
	last_stats.size   = (double)size;

	/* store them here for hashing whose stats we should believe when the
	 * user requests them */
	FT_SESSION(c)->stats.users  = last_stats.users;
	FT_SESSION(c)->stats.shares = last_stats.shares;
	FT_SESSION(c)->stats.size   = last_stats.size;
}
