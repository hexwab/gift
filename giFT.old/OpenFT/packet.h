/*
 * packet.h
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

#ifndef __PACKET_H__
#define __PACKET_H__

/*****************************************************************************/

#include <stdarg.h>

/**************************************************************************/

#define FT_HEADER_LEN (sizeof(ft_uint16)+sizeof(ft_uint16))

typedef struct
{
	int offset;   /* used internally, skipped over when writing */

	ft_uint16 len;
	ft_uint16 command;

	char data[1]; /* pointer trickery.  len bytes after this are still our
				   * valid memory segment */
} FTPacket;

/**************************************************************************/

FTPacket *ft_packet_new     (ft_uint16 ft_command, char *data, int len);
void      ft_packet_free    (FTPacket *packet);

char     *ft_packet_data    (char *fmt, va_list args, int *len, char *input);
ft_uint32 ft_packet_get_int (FTPacket *packet, size_t size, int host_order);
ft_uint32 ft_packet_get_packed_int (FTPacket *packet);
char     *ft_packet_get_str (FTPacket *packet);

/*****************************************************************************/

int ft_packet_send_constructed (Connection *nc, FTPacket *packet);
int ft_packet_send             (Connection *c, unsigned short ft_command,
                                char *fmt, ...);
int ft_packet_send_indirect    (unsigned long deliver_to,
                                unsigned short ft_command, char *fmt, ...);

/**************************************************************************/

#endif /* __PACKET_H__ */
