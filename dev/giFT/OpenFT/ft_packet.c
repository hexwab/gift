/*
 * ft_packet.c
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

#include "ft_netorg.h"
#include "ft_packet.h"

/*****************************************************************************/

/* #define FT_PACKET_DEBUG */

/* bitwise operators for fun an profit */
#define COMMAND_MASK (~((unsigned short)(FT_PACKET_STREAM)))
#define FLAGS_MASK (~COMMAND_MASK)

/*****************************************************************************/

/* make sure packet->data is large enough to hold len bytes */
static int packet_resize (FTPacket *packet, size_t len)
{
	unsigned char *resize;
	size_t         resize_len;

	if (!packet)
		return FALSE;

	/* realloc (..., 0) == free */
	if (len == 0)
	{
		free (packet->data);
		packet->data_len = 0;

		return TRUE;
	}

	assert (len >= FT_PACKET_HEADER);
	assert (len <= FT_PACKET_HEADER + FT_PACKET_MAX);

	/* the buffer we have allocated is already large enough */
	if (packet->data_len >= len)
		return TRUE;

	/* determine an appropriate resize length */
	for (resize_len = packet->data_len; resize_len < len; )
	{
		if (resize_len == 0)
			resize_len = FT_PACKET_INITIAL;
		else
			resize_len *= 2;
	}

	/* gracefully fail if we are unable to resize the buffer */
	if (!(resize = realloc (packet->data, resize_len)))
		return FALSE;

#ifdef DEBUG
	/* zero the newly allocated data for debugging purposes */
	memset (resize + packet->data_len, 0, resize_len - packet->data_len);
#endif /* DEBUG */

	/* modify the packet structure to reflect this resize */
	packet->data = resize;
	packet->data_len = resize_len;

	return TRUE;
}

FTPacket *ft_packet_new (ft_uint16 cmd, ft_uint16 flags)
{
	FTPacket *packet;

	if (!(packet = MALLOC (sizeof (FTPacket))))
		return NULL;

	ft_packet_set_command (packet, cmd);
	ft_packet_set_flags (packet, flags);
	ft_packet_set_length (packet, 0);

#if 0
	/* set the initial data buffer size */
	if (!packet_resize (packet, FT_PACKET_INITIAL))
	{
		ft_packet_free (packet);
		return NULL;
	}
#endif

#ifdef FT_PACKET_DEBUG
	TRACE (("%s", ft_packet_fmt (packet)));
#endif /* FT_PACKET_DEBUG */

	return packet;
}

FTPacket *ft_packet_dup (FTPacket *packet)
{
	FTPacket *n;

	if (!packet)
		return NULL;

	if (!(n = ft_packet_new (ft_packet_command (packet), ft_packet_flags (packet))))
		return NULL;

	ft_packet_set_length (n, ft_packet_length (packet));

	if (!packet_resize (packet, FT_PACKET_HEADER + ft_packet_length (n)))
	{
		ft_packet_free (n);
		return NULL;
	}

	/* make sure we don't have any garbage data in the header and then
	 * write the content stream from the supplied packet */
	memset (n->data, 0, FT_PACKET_HEADER);
	memcpy (n->data + FT_PACKET_HEADER, packet->data, ft_packet_length (n));

	return n;
}

void ft_packet_free (FTPacket *packet)
{
	if (!packet)
		return;

#ifdef FT_PACKET_DEBUG
	TRACE (("%s", ft_packet_fmt (packet)));
#endif /* FT_PACKET_DEBUG */

	free (packet->data);
	free (packet);
}

/*****************************************************************************/

/*
 * Serialize a constructed packet, returning the data stream which
 * can be transacted over the OpenFT connection.  This should only be
 * called just before packet delivery.
 */
unsigned char *ft_packet_serialize (FTPacket *packet, size_t *s_len)
{
	ft_uint16 cmd;
	ft_uint16 len;

	if (!packet)
		return NULL;

	len = ft_packet_length (packet);
	cmd = ft_packet_command (packet) | ft_packet_flags (packet);

	len = (ft_uint16) htons ((unsigned short)len);
	cmd = (ft_uint16) htons ((unsigned short)cmd);

	/* packet_resize will never shrink the size of the packet, so this is
	 * being abused as a min clamp */
	if (!packet_resize (packet, FT_PACKET_HEADER))
		return NULL;

	memcpy (packet->data, &len, sizeof (len));
	memcpy (packet->data + sizeof (len), &cmd, sizeof (cmd));

	/* set the total serialized length, if requested */
	if (s_len)
		*s_len = FT_PACKET_HEADER + ft_packet_length (packet);

	return packet->data;
}

/*
 * Create a new packet instance from recv'd data.  Note that the serialized
 * data stream contains a copy of the length/command as to -cleanly- provide
 * both a linear stream for efficient serialization but also a structured C
 * API for referencing these values.
 */
FTPacket *ft_packet_unserialize (unsigned char *data, size_t data_len)
{
	FTPacket *packet;
	ft_uint16 cmd;
	ft_uint16 len;

	if (data_len < FT_PACKET_HEADER)
		return NULL;

	len = net_get16 (data, TRUE);
	cmd = net_get16 (data + 2, TRUE);

	/* we were not given enough data */
	if (len > FT_PACKET_MAX || len + FT_PACKET_HEADER > data_len)
		return NULL;

	if (!(packet = ft_packet_new (cmd, cmd)))
		return NULL;

	/* copy the complete packet into the data stream */
	ft_packet_set_length (packet, len);

	if (!packet_resize (packet, FT_PACKET_HEADER + ft_packet_length (packet)))
	{
		ft_packet_free (packet);
		return NULL;
	}

	memcpy (packet->data, data, FT_PACKET_HEADER + ft_packet_length (packet));

	return packet;
}

/*****************************************************************************/

void ft_packet_set_command (FTPacket *packet, ft_uint16 cmd)
{
	packet->command &= ~COMMAND_MASK;
	packet->command |= (cmd & COMMAND_MASK);
}

void ft_packet_set_flags (FTPacket *packet, ft_uint16 flags)
{
	packet->command &= ~FLAGS_MASK;
	packet->command |= (flags & FLAGS_MASK);
}

void ft_packet_set_length (FTPacket *packet, ft_uint16 len)
{
	if (len > FT_PACKET_MAX)
		return;

	packet->len = len;
}

ft_uint16 ft_packet_command (FTPacket *packet)
{
	if (!packet)
		return 0;

	return (packet->command & COMMAND_MASK);
}

ft_uint16 ft_packet_flags (FTPacket *packet)
{
	if (!packet)
		return 0;

	return (packet->command & FLAGS_MASK);
}

ft_uint16 ft_packet_length (FTPacket *packet)
{
	if (!packet)
		return 0;

	return packet->len;
}

/*****************************************************************************/

static int packet_append (FTPacket *packet, void *data, size_t size)
{
	size_t required;

	if (!packet || !data || size == 0)
		return FALSE;

	if (packet->len + size > FT_PACKET_HEADER + FT_PACKET_MAX)
		return FALSE;

	/* determine the total required space to append size bytes */
	required = FT_PACKET_HEADER + ft_packet_length (packet) + size;

	/* if we can't resize, gracefully fail...the caller should determine
	 * how bad this truly is */
	if (!packet_resize (packet, required))
		return FALSE;

	/* append size bytes of data */
	memcpy (packet->data + FT_PACKET_HEADER + ft_packet_length (packet),
	        data, size);

	/* packet->len += size */
	ft_packet_set_length (packet, ft_packet_length (packet) + size);
	return TRUE;
}

int ft_packet_put_uint (FTPacket *packet, void *data, size_t size,
                        int host_order)
{
	int ret;
	ft_uint16 unet16;
	ft_uint32 unet32;

	if (!data || size < 0 || size > sizeof (ft_uint32))
		return FALSE;

	switch (size)
	{
	 case 2:
		unet16 = net_get16 (data, host_order);
		ret = packet_append (packet, &unet16, size);
		break;
	 case 4:
		unet32 = net_get32 (data, host_order);
		ret = packet_append (packet, &unet32, size);
		break;
	 default:
		ret = packet_append (packet, data, size);
		break;
	}

	return ret;
}

int ft_packet_put_uint8 (FTPacket *packet, ft_uint8 data)
{ return ft_packet_put_uint (packet, &data, sizeof (data), FALSE); }

int ft_packet_put_uint16 (FTPacket *packet, ft_uint16 data, int host_order)
{ return ft_packet_put_uint (packet, &data, sizeof (data), host_order); }

int ft_packet_put_uint32 (FTPacket *packet, ft_uint32 data, int host_order)
{ return ft_packet_put_uint (packet, &data, sizeof (data), host_order); }

int ft_packet_put_ip (FTPacket *packet, in_addr_t ip)
{
	if (!ft_packet_put_uint16 (packet, (ft_uint16)4, TRUE))
		return FALSE;
	if (!ft_packet_put_uint32 (packet, (ft_uint32)ip, FALSE))
		return FALSE;

	return TRUE;
}

int ft_packet_put_ustr (FTPacket *packet, unsigned char *str, int len)
{
	if (!str || len == 0)
		return ft_packet_put_uint8 (packet, 0);

	if (len == -1)
		len = strlen (str) + 1;

	return packet_append (packet, str, (size_t)len);
}

int ft_packet_put_str (FTPacket *packet, char *str)
{ return ft_packet_put_ustr (packet, (unsigned char *)str, -1); }

/*****************************************************************************/

ft_uint32 ft_packet_get_uint (FTPacket *packet, size_t size, int host_order)
{
	ft_uint32      data32 = 0;
	unsigned char *p;

	if (!packet || size == 0 || size > sizeof (ft_uint32))
		return 0;

	/* check overrun */
	if (packet->offset + size > packet->len)
		return 0;

	p = packet->data + FT_PACKET_HEADER + packet->offset;

	switch (size)
	{
	 case 1:
		data32 = (ft_uint32)net_get8 (p);
		break;
	 case 2:
		data32 = (ft_uint32)net_get16 (p, host_order);
		break;
	 case 4:
		data32 = net_get32 (p, host_order);
		break;
	 default:
		abort ();
		break;
	}

	packet->offset += size;

	return data32;
}

ft_uint8 ft_packet_get_uint8 (FTPacket *packet)
{ return (ft_uint8)ft_packet_get_uint (packet, 1, FALSE); }

ft_uint16 ft_packet_get_uint16 (FTPacket *packet, int host_order)
{ return (ft_uint16)ft_packet_get_uint (packet, 2, host_order); }

ft_uint32 ft_packet_get_uint32 (FTPacket *packet, int host_order)
{ return ft_packet_get_uint (packet, 4, host_order); }

in_addr_t ft_packet_get_ip (FTPacket *packet)
{
	ft_uint16 ip_ver;
	ft_uint32 ip;

	if (!(ip_ver = ft_packet_get_uint16 (packet, TRUE)))
		return 0;

	if (ip_ver != 4)
	{
		GIFT_WARN (("invalid ip version: %hu", ip_ver));
		return 0;
	}

	/* the address is already in network order, do not call ntohl... */
	ip = ft_packet_get_uint32 (packet, FALSE);

	return (in_addr_t)ip;
}

/* checks for the arrays sentinel (defined as an array of zeroes size long)
 * TODO: there are much faster ways of doing this */
static int array_sentinel (char *ptr, size_t size, size_t *nmemb)
{
	/* if a total number of elements was supplied, use that value instead
	 * of the data sentinel checks */
	if (nmemb)
	{
		/* no members left, return as if we hit a sentinel */
		if (*nmemb == 0)
			return TRUE;

		(*nmemb)--;
		return FALSE;
	}

	while (size--)
	{
		/* non-zero byte, this element couldnt be the sentinel */
		if (*ptr != 0)
			return FALSE;

		ptr++;
	}

	return TRUE;
}

/* receive a linear array from the socket, requiring a sentinel size bytes
 * long or that you supply a total number of elements to read */
void *ft_packet_get_array (FTPacket *packet, size_t size, size_t nmemb,
                           int host_order)
{
	char   *start;
	char   *ptr;
	char   *end;
	size_t *nmemb_ptr = NULL;

	if (!packet || size == 0)
		return NULL;

	if (packet->offset >= packet->len)
		return NULL;

	/* preserve the value of nmemb in the laziest way possible */
	if (nmemb)
		nmemb_ptr = &nmemb;

	start = packet->data + FT_PACKET_HEADER + packet->offset;
	end   = packet->data + FT_PACKET_HEADER + packet->len;

	/* loop the packet data, processing host order conversion and checking
	 * for the existence of the sentinel before we return */
	for (ptr = start; ; ptr += size)
	{
		/* error in data stream, requested a read beyond total received
		 * bytes, error condition will be caught outside of this loop */
		if (ptr + size >= end)
			break;

		/* check if we have cleanly finished reading the available data */
		if (array_sentinel (ptr, size, nmemb_ptr))
			break;

		if (host_order)
		{
			switch (size)
			{
			 case 2:
				net_put16 (ptr, net_get16 (ptr, TRUE));
				break;
			 case 4:
				net_put32 (ptr, net_get32 (ptr, TRUE));
				break;
			}
		}
	}

	/* invalid data, no sentinel found...set this packets eof to avoid any
	 * possible data corruption if the packet is still salvageable (which
	 * will be determined by the caller, not us) */
	if (ptr + size > end)
	{
		packet->offset = packet->len;
		return NULL;
	}

	if (!nmemb_ptr)
	{
		/* ptr is at the sentinel right now, move one element out to calculate
		 * the next packet start */
		ptr += size;
	}

	/* ptr - start represents the length of the entire array plus sentinel
	 * (if one existed) at this point */
	packet->offset += (ptr - start);

	return start;
}

unsigned char *ft_packet_get_ustr (FTPacket *packet, size_t len)
{ return ft_packet_get_array (packet, 1, len, FALSE); }

char *ft_packet_get_str (FTPacket *packet)
{ return ft_packet_get_array (packet, 1, 0, FALSE); }

/*****************************************************************************/

static int send_packet (Connection *c, FTPacket *packet, void *udata)
{
	unsigned char *data;
	size_t         len = 0;

	if (!packet)
		return FALSE;

	if (!(data = ft_packet_serialize (packet, &len)))
		return -1;

	assert (len > 0);

#ifdef FT_PACKET_DEBUG
	TRACE_SOCK (("%s", ft_packet_fmt (packet)));
#endif /* FT_PACKET_DEBUG */

	net_send (c->fd, data, len);
	return FALSE;
}

static int destroy_packet (Connection *c, FTPacket *packet, void *udata)
{
#ifdef FT_PACKET_DEBUG
	TRACE_SOCK (("destroying %s", ft_packet_fmt (packet)));
#endif /* FT_PACKET_DEBUG */

	ft_packet_free (packet);
	return FALSE;
}

int ft_packet_send (Connection *c, FTPacket *packet)
{
	if (!c || c->fd < 0)
	{
		ft_packet_free (packet);
		return -1;
	}

	/* if this is not an appropriate time during the handshaking to deliver
	 * this kind of packet, we will queue it up in the session subsystem and
	 * deliver once handshaking is complete */
	if (ft_session_queue (c, packet))
		return 0;

#ifdef FT_PACKET_DEBUG
	TRACE_SOCK (("sending %s", ft_packet_fmt (packet)));
#endif /* FT_PACKET_DEBUG */

	/* add this packet to the queue, writing when select indicates a write
	 * status change */
	queue_add_single (c,
	                  (QueueWriteFunc) send_packet,
	                  (QueueWriteFunc) destroy_packet,
	                  packet, NULL);

	return 0;
}

static int locate_to (FTNode *node, in_addr_t *ip)
{
	/* we request info on a specific ip address and upon receiving this
	 * information check to see if the user has any packets that need to be
	 * written...kind of a hack */
	ft_packet_sendva (FT_CONN(node), FT_NODEINFO_REQUEST, 0, "I", *ip);
	return TRUE;
}

int ft_packet_sendto (in_addr_t to, FTPacket *packet)
{
	FTNode *node;

	/* make sure that we have a node associated this with address so that we
	 * can store the temporary packets if necessary */
	if (!(node = ft_node_register (to)))
		return -1;

	/*
	 * Attempt to locate a currently active connection with this node.  If
	 * this fails (which is pretty likely if you're not a search node), then
	 * we will attempt to contact all remote search nodes requesting more
	 * contact information for this user.  When that information returns,
	 * we will check the temporary lookup data we assign here and finish
	 * the packet delivery.
	 */
	if (!FT_CONN(node))
	{
		/* queue this packet for delivery */
		ft_node_queue (node, packet);

		/*
		 * Ask all our currently active search node connections if they are
		 * aware of how to contact this node.
		 *
		 * NOTE: Eventually, the search cluster should have the ability to
		 * forward these requests through the network, implementing a genuine
		 * routing protocol).
		 */
		ft_netorg_foreach (NODE_SEARCH, NODE_CONNECTED, 0,
		                   FT_NETORG_FOREACH(locate_to), &to);

		/* we haven't delivered anything yet, but may eventually */
		return 0;
	}

	/* send the message directly to this connection */
	return ft_packet_send (FT_CONN(node), packet);
}

/*****************************************************************************/

void put_uint32_array (FTPacket *packet, ft_uint32 *array)
{
	for (; array && *array; array++)
		ft_packet_put_uint32 (packet, *array, TRUE);

	ft_packet_put_uint32 (packet, 0, TRUE);
}

int ft_packet_sendva (Connection *c, ft_uint16 cmd, ft_uint16 flags,
                      char *fmt, ...)
{
	FTPacket *packet;
	va_list   args;

	if (!(packet = ft_packet_new (cmd, flags)))
		return -1;

	va_start (args, fmt);

	for (; fmt && *fmt; fmt++)
	{
		switch (*fmt)
		{
		 case 'l':
			ft_packet_put_uint32 (packet, (ft_uint32)va_arg (args, long), TRUE);
			break;
		 case 'L':
			put_uint32_array (packet, va_arg (args, ft_uint32 *));
			break;
		 case 'h':
			ft_packet_put_uint16 (packet, (ft_uint16)va_arg (args, long), TRUE);
			break;
		 case 'c':
			ft_packet_put_uint8 (packet, (ft_uint8)va_arg (args, long));
			break;
		 case 's':
			ft_packet_put_str (packet, va_arg (args, char *));
			break;
		 case 'S':
			{
				unsigned char *data = va_arg (args, unsigned char *);
				size_t         len  = (size_t)va_arg (args, long);

				ft_packet_put_ustr (packet, data, len);
			}
			break;
		 case 'I':
			ft_packet_put_ip (packet, (in_addr_t)va_arg (args, long));
			break;
		 default:
			abort ();
			break;
		}
	}

	va_end (args);

	return ft_packet_send (c, packet);
}

/*****************************************************************************/

char *ft_packet_fmt (FTPacket *packet)
{
	static char buf[512];

	if (!packet)
		return NULL;

	snprintf (buf, sizeof (buf) - 1, "%04hu:%04hu",
	         (unsigned short)ft_packet_length (packet),
	         (unsigned short)ft_packet_command (packet));

	return buf;
}
