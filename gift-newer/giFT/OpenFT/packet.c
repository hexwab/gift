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

#define STR_APPEND(str,len,a_len,data,data_len) \
	str = str_append (str, &len, &a_len, (const void *) data, data_len)

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

static char *str_append (char *str, size_t *str_len, size_t *a_len,
                         const void *data, size_t data_len)
{
	/* apply double buffer reallocation if needed */
	if (*str_len + data_len > *a_len)
	{
		char *str_resize;

		if (*a_len == 0)
			*a_len = 128;

		/* keep doubling until it's large enough */
		while (*str_len + data_len > *a_len)
			*a_len *= 2;

		if (!(str_resize = realloc (str, *a_len)))
		{
			free (str);
			return NULL;
		}

		str = str_resize;
	}

	memcpy (str + (*str_len), data, data_len);
	*str_len += data_len;

	return str;
}

/*
 * construct the data string for an OpenFT packet
 *
 * printf-like arguments translate to raw binary net types
 *
 * giFT extensions:
 *
 *   *FMT uses a linear array w/ a NULL sentinel.  %s and *c would be equiv.
 *   +FMT arbitrary extensions we provide...doesnt mean a damn thing :)
 *
 */
static char *ft_packet_data (char *fmt, va_list args,
                             ft_uint16 *command, int *len)
{
	char   c, type;
	char  *output;
	size_t out_len = 0;
	size_t a_len   = 128;
	int    comp = FALSE; /* compressed */

	assert (fmt);
	assert (*fmt);

	output = malloc (a_len);

	/* this is a fuckin mess */
	while (*fmt)
	{
		type = *fmt++;

		switch (type)
		{
		 case '%': /* single type, short long, string */
			{
				ft_uint16 us;
				ft_uint32 ul;
				char     *str;

				c = *fmt++;

				switch (c)
				{
				 case 'h': /* short */
					us = htons ((unsigned short) va_arg (args, long));
					STR_APPEND (output, out_len, a_len, &us, sizeof (us));
					fmt++;
					break;
				 case 'l': /* long */
					ul = htonl ((unsigned long) va_arg (args, long));
					STR_APPEND (output, out_len, a_len, &ul, sizeof (ul));
					fmt++;
					break;
				 case 's': /* ascii string */
					if (!(str = va_arg (args, char *)))
						str = "";
					/* include \0 */
					STR_APPEND (output, out_len, a_len, str, strlen (str) + 1);
					break;
				}
			}
			break;
		 case '*': /* array.  sentinel is always 0 */
			{
				ft_uint32 *ul;
				ft_uint32  net_ul;

				c = *fmt++;

				switch (c)
				{
				 case 'l': /* long */
					ul = (ft_uint32 *) va_arg (args, long *);
					for (;; ul++)
					{
						net_ul = htonl (*ul);
						STR_APPEND (output, out_len, a_len,
						            &net_ul, sizeof (net_ul));

						if (*ul == 0)
							break;
					}
					fmt++;
					break;
				}
			}
			break;
		 case '+': /* arbitrary extensions */
			{
				ft_uint32  ul;
				char      *str;

				c = *fmt++;

				switch (c)
				{
				 case 'C': /* compressed data segment */
					comp = TRUE;
					/* no break; */
				 case 'S': /* data segment */
					str = va_arg (args, char *);
					ul = (ft_uint32) va_arg (args, long);
					if (str && ul)
						STR_APPEND (output, out_len, a_len, str, ul);
					break;
				}
			}
		 default:
			break;
		}
	}

	if (len)
		*len = out_len;

#ifdef USE_ZLIB
	if (comp && command)
	{
		/* set the compression bit */
		*command |= FT_PACKET_COMPRESSED;
	}
#endif /* USE_ZLIB */

	/* we now have a string ready for network transmission */
	return output;
}

/*****************************************************************************/

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
		TRACE (("invalid size %i", size));
		return data_32;
	}

	packet->offset += size;

	return data_32;
}

/* this function is optimized to use the same memory available from the
 * socket */
void *ft_packet_get_array (FTPacket *packet, size_t size, int host_order)
{
	char *start;
	char *ptr;
	char *end;

	assert (packet);

	if (packet->offset >= packet->len)
		return NULL;

	start = packet->data + packet->offset;
	end   = packet->data + packet->len;

	/* TODO - optimize compares inside this loop */
	for (ptr = start; ptr + size < end && *ptr; ptr += size)
	{
		if (host_order)
		{
			switch (size)
			{
			 case 2:
				*ptr = ntohs (*ptr);
				break;
			 case 4:
				*ptr = ntohl (*ptr);
				break;
			 default:
				break;
			}
		}
	}

	/* invalid data...no sentinel found */
	if (ptr + size > end)
	{
		packet->offset = packet->len;
		return NULL;
	}

	/* ptr is at the sentinel right now, move one element out to calculate the
	 * next packet start */
	ptr += size;

	/* this includes the sentinel now */
	packet->offset += (ptr - start);

	return start;
}

char *ft_packet_get_str (FTPacket *packet)
{
	return ft_packet_get_array (packet, 1, FALSE);
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
	packet->len     = htons (packet->len);
	packet->command = htons (packet->command);

	/* packet->len is the start of the data stream that is expected to be
	 * written out */
	ret = net_send (c->fd, (char *) &packet->len, len + FT_HEADER_LEN);

	/* convert back */
	packet->len     = ntohs (packet->len);
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

	if (!c || c->fd < 0)
		return -1;

	if (fmt)
	{
		va_start (args, fmt);
		data = ft_packet_data (fmt, args, &ft_command, &len);
		va_end (args);
	}

	return ft_packet_send_data (c, ft_command, data, len);
}

int ft_packet_append (FTPacket **packet, char *fmt, ...)
{
	FTPacket *packet_new;
	va_list   args;
	char     *data = NULL;
	int       len  = 0;

	if (!packet || !*packet)
		return FALSE;

	/* we technically did what they asked :) */
	if (!fmt)
		return TRUE;

	va_start (args, fmt);
	data = ft_packet_data (fmt, args, &((*packet)->command), &len);
	va_end (args);

	if (!len)
	{
		free (data);
		return TRUE;
	}

	/* TODO -- this badly needs opting */
	packet_new = realloc (*packet, sizeof (FTPacket) + (*packet)->len + len);

	if (!packet_new)
		return FALSE;

	/* write the new data onto the packet buffer */
	memcpy (packet_new->data + packet_new->len, data, len);
	free (data);

	packet_new->len += len;

	*packet = packet_new;

	return TRUE;
}

/*****************************************************************************/

static Connection *locate_node (Connection *c, Node *node,
								unsigned long *deliver_to)
{
	if (node->ip == P_INT (deliver_to))
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
	c = conn_foreach ((ConnForeachFunc) locate_node, I_PTR (deliver_to),
	                  NODE_USER, NODE_CONNECTED, 0);

	if (!c)
		return -1;

	if (fmt)
	{
		va_start (args, fmt);
		data = ft_packet_data (fmt, args, &ft_command, &len);
		va_end (args);
	}

	return ft_packet_send_data (c, ft_command, data, len);
}
