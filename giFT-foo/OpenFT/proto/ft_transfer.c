/*
 * $Id: ft_transfer.c,v 1.2 2003/05/26 11:47:40 jasta Exp $
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

#include "ft_xfer.h"
#include "ft_http_client.h"

#include "ft_protocol.h"
#include "ft_transfer.h"

/*****************************************************************************/

FT_HANDLER (ft_push_request)
{
	in_addr_t  ip;
	in_port_t  port;
	char      *file;
	off_t      start;
	off_t      stop;

	ip    =        ft_packet_get_ip     (packet);
	port  =        ft_packet_get_uint16 (packet, TRUE);
	file  =        ft_packet_get_str    (packet);
	start = (off_t)ft_packet_get_uint32 (packet, TRUE);
	stop  = (off_t)ft_packet_get_uint32 (packet, TRUE);

	/* ip || port == 0 in this instance means that we are to send this file
	 * back to the search node contacting us here */
	if (ip == 0 || port == 0)
	{
		ip   = FT_NODE(c)->ip;
		port = FT_NODE(c)->http_port;
	}

	/* sanity check on the data received */
	if (!file || stop == 0)
	{
		FT->DBGSOCK (FT, c, "incomplete request");
		return;
	}

	/* make an outgoing http connection and advertise that we can fulfill
	 * the push request */
	http_client_push (ip, port, file, start, stop);
}

FT_HANDLER (ft_push_forward)
{
	FTPacket *fwd;
	in_addr_t ip;
	char     *request;
	uint32_t  start;
	uint32_t  stop;

	ip      = ft_packet_get_ip     (packet);
	request = ft_packet_get_str    (packet);
	start   = ft_packet_get_uint32 (packet, TRUE);
	stop    = ft_packet_get_uint32 (packet, TRUE);

	if (!request || stop == 0)
	{
		FT->DBGSOCK (FT, c, "incompleted request");
		return;
	}

	if (!(fwd = ft_packet_new (FT_PUSH_REQUEST, 0)))
		return;

	/* reconstruct the complete request using the directly connected peers
	 * address information */
	ft_packet_put_ip     (fwd, FT_NODE(c)->ip);
	ft_packet_put_uint16 (fwd, FT_NODE(c)->http_port, TRUE);

	/* put the original data back on */
	ft_packet_put_str    (fwd, request);
	ft_packet_put_uint32 (fwd, start, TRUE);
	ft_packet_put_uint32 (fwd, stop, TRUE);

	/*
	 * Pass this data along to the user who will be actually sending the
	 * file to the user making this request.
	 *
	 * NOTE:
	 *
	 * No further relayed communication will take place.  If successful, the
	 * node will connect back to the user and advertise the file, otherwise
	 * it will just be dropped off into oblivion right here.  Life is rough,
	 * eh? :)
	 */
	ft_packet_sendto (ip, fwd);
}
