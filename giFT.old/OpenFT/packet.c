/*
 * packet.c
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

#include "openft.h"

#include "netorg.h"

/**************************************************************************/

#define PACKET_APPEND(str,len,data,data_len) \
        if (len + data_len < 4096) \
        { \
        	memcpy (str + len, data, data_len); \
        	len += data_len; \
		}

/*****************************************************************************/

/*
 * construct a new OpenFT packet
 */
FTPacket *ft_packet_new (ft_uint16 ft_command, char *data, int len)
{
	FTPacket *packet;

	assert (len >= 0);

	/* allocate enough size to include the request with the structure in
	 * a linear fashion */
	packet = malloc (sizeof (FTPacket) + len);

	packet->len     = len;
	packet->command = ft_command;
	packet->offset  = 0;

	/* move the data onto the structures buffer */
	if (data)
	{
		memcpy (packet->data, data, len);
		memset (packet->data + len, 0, sizeof (char));
	}

#if 0
	printf ("packet_new: %hu %hu { ", len, ft_command);

	{
		int i;
		for (i = 0; i < packet->len; i++)
			printf ("0x%02x ", packet->data[i]);
	}

	printf ("}\n");
#endif

	return packet;
}


/*
 * clean up.
 */
void ft_packet_free (FTPacket *packet)
{
	free (packet);
}

/*****************************************************************************/

/*
 * construct the data string for an OpenFT packet
 *
 * printf-like arguments translate to raw binary net types
 *
 * TODO - optimize this
 */
char *ft_packet_data (char *fmt, va_list args, int *len, char *input)
{
	char  c;
	char *output;
	int   out_len;
	char *str;
	ft_uint32 ul;
	ft_uint16 us;

	if (input)
	{
		output = input;
		out_len = *len;
	} else {
		output = malloc (4096);
		out_len = 0;
	}

	assert (fmt);
	assert (*fmt);

	while (*fmt)
	{
		if (*fmt++ == '%')
		{
			/* *fmt will be the possible long opt, c being the modifier */
			c = *fmt++;

			switch (c)
			{
			 case 'h': /* short */
				us = htons ((unsigned short) va_arg (args, long));
				PACKET_APPEND (output, out_len, &us, sizeof (us));
				fmt++;
				break;
			 case 'l': /* long */
				ul = htonl ((unsigned long) va_arg (args, long));
				PACKET_APPEND (output, out_len, &ul, sizeof (ul));
				fmt++;
				break;
			 case 's': /* ascii string */
				if (!(str = va_arg (args, char *)))
					str = "";
				/* include \0 */
				PACKET_APPEND (output, out_len, str, strlen (str) + 1);
				break;
			case 'p': /* packed int */
				ul = va_arg (args, unsigned long);
				{
					unsigned long i;
					unsigned char buf[8],*ptr=buf;
					int len=1;
					i=ul;
					while (i>127)
					{
						len++;
						i>>=7;
					}
					
					ptr+=len;
					i=ul;
					while (i>127)
					{
						*(--ptr)=(i &0x7f)|0x80;
						i>>=7;
					}
					*(--ptr)=i |0x80;
					ptr[len-1]&=0x7f;
					PACKET_APPEND (output, out_len, buf, len);

					TRACE(("sending packed int 0x%x, len %d",ul, len));
				}
				break;
			}
		}
	}

	if (len)
		*len = out_len;

	/* we now have a string ready for network transmission */
	return output;
}

ft_uint32 ft_packet_get_int (FTPacket *packet, size_t size, int host_order)
{
	char      data_8;
	ft_uint16 data_16;
	ft_uint32 data_32 = 0;
	char     *offs;

	assert (packet);

	/* check overrun */
	if (packet->offset + size > packet->len)
		return 0;

	offs = packet->data + packet->offset;

	switch (size)
	{
	 case 1:
		memcpy (&data_8, offs, size);
		data_32 = (ft_uint32) data_8;
		break;
	 case 2:
		memcpy (&data_16, offs, size);
		data_32 = (ft_uint32) ((host_order ? ntohs (data_16) : data_16));
		break;
	 case 4:
		memcpy (&data_32, offs, size);
		if (host_order)
			data_32 = ntohl (data_32);
		break;
	 default:
		printf ("%s: wtf are you doing?\n", __PRETTY_FUNCTION__);
		return data_32;
	}

	packet->offset += size;

	return data_32;
}

char *ft_packet_get_str (FTPacket *packet)
{
	char *start, *ptr;
	int i;

	assert (packet);

	if (packet->offset == packet->len)
		return NULL;

	/* we have to write our own strlen function so that we can't be DoS'd
	 * attack of the script kiddies can really hurt :) */
	i      = packet->len - packet->offset;
	start  = ptr = packet->data + packet->offset;

	while (*ptr++ && i-- > 0);

	/* we ran out of data before we found the \0, treat this as an invalid
	 * packet */
	if (i <= 0)
	{
		packet->offset = packet->len;  /* no subsequent calls will allow this
		                                * packet anymore */
		return NULL;
	}

	/* length is (ptr - start), including the \0 */
	packet->offset += (ptr - start);

	return start;
}



ft_uint32 ft_packet_get_packed_int (FTPacket *packet)
{
	ft_uint32 data = 0;
	char *start, *ptr;
	int i;

	assert (packet);

	i      = packet->len - packet->offset;
	start  = ptr = packet->data + packet->offset;

	while (i-- > 0)
	{
		data <<= 7;
		data |= (*ptr & 0x7f);
		if (!(*ptr++ & 0x80))
			break;
	}

	if (i <= 0)
	{
		packet->offset = packet->len;  /* no subsequent calls will allow this
		                                * packet anymore */
		TRACE(("truncated packet"));
		return 0;
	}

	packet->offset += (ptr-start);

	TRACE(("returning %u - %u",data, start-ptr));
	return data;
}



/*****************************************************************************/

/*
 * sends a constructed packet
 */
int ft_packet_send_constructed (Connection *c, FTPacket *packet)
{
	int ret, len;

	/* nothing to send, piss off */
	if (!packet)
		return FALSE;

	len = packet->len;

	/* we can't blindly send this data yet, it's in host order */
	packet->len = htons (packet->len);
	packet->command = htons (packet->command);

	/* packet->len is the start of the data stream that is expected to be
	 * written out */
	ret = send (c->fd, &packet->len, len + FT_HEADER_LEN, 0);

	/* convert back */
	packet->len = ntohs (packet->len);
	packet->command = ntohs (packet->command);

	return ret;
}

/*****************************************************************************/

static int ft_packet_send_data (Connection *c, unsigned short ft_command,
                              char *data, int len)
{
	FTPacket *packet;

	packet = ft_packet_new (ft_command, data, len);

	free (data);

	if (!packet)
		return FALSE;

	/* we aren't going to manipulate this buffer, so send it */
	ft_packet_send_constructed (c, packet);
	ft_packet_free (packet);

	return TRUE;
}

/*
 * send a command to the supplied node connection
 *
 * request may be NULL for a 0 length packet ( + 4 bytes for length+command)
 */
int ft_packet_send (Connection *c, unsigned short ft_command, char *fmt, ...)
{
	va_list args;
	char *data = NULL;
	int   len  = 0;

	if (fmt)
	{
		va_start (args, fmt);
		data = ft_packet_data (fmt, args, &len, NULL);
		va_end (args);
	}

	return ft_packet_send_data (c, ft_command, data, len);
}

/*****************************************************************************/

static Connection *locate_node (Connection *c, Node *node,
								unsigned long deliver_to)
{
	if (node->ip == deliver_to)
		return c;

	return NULL;
}

/*
 * send a message to the supplied child
 */
int ft_packet_send_indirect (unsigned long deliver_to,
                             unsigned short ft_command, char *fmt, ...)
{
	Connection *c;
	va_list args;
	char *data = NULL;
	int   len  = 0;

	/* locate the connection to send this to */
	c = conn_foreach ((ConnForeachFunc) locate_node, (void *) deliver_to,
	                  NODE_USER, NODE_CONNECTED, 0);

	if (!c)
		return -1;

	if (fmt)
	{
		va_start (args, fmt);
		data = ft_packet_data (fmt, args, &len, NULL);
		va_end (args);
	}

	return ft_packet_send_data (c, ft_command, data, len);
}
