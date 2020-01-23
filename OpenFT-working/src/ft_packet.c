/*
 * $Id: ft_packet.c,v 1.48 2004/07/19 22:20:48 jasta Exp $
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

#include "ft_netorg.h"
#include "ft_packet.h"

/*****************************************************************************/

/* bitwise operators for fun an profit */
#define FT_COMMAND_MASK (~((uint16_t)(FT_PACKET_STREAM)))
#define FT_FLAGS_MASK (~FT_COMMAND_MASK)

/* initial allocation size */
#define FT_PACKET_INITIAL 128

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

#ifdef OPENFT_DEBUG
	/* zero the newly allocated data for debugging purposes */
	memset (resize + packet->data_len, 0, resize_len - packet->data_len);
#endif /* OPENFT_DEBUG */

	/* modify the packet structure to reflect this resize */
	packet->data = resize;
	packet->data_len = resize_len;

	return TRUE;
}

FTPacket *ft_packet_new (uint16_t cmd, uint16_t flags)
{
	FTPacket *packet;

	if (!(packet = MALLOC (sizeof (FTPacket))))
		return NULL;

	ft_packet_set_command (packet, cmd);
	ft_packet_set_flags (packet, flags);
	ft_packet_set_length (packet, 0);

	packet->overrun = 0;

#if 0
	/* set the initial data buffer size */
	if (!packet_resize (packet, FT_PACKET_INITIAL))
	{
		ft_packet_free (packet);
		return NULL;
	}
#endif

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
	uint16_t cmd;
	uint16_t len;

	if (!packet)
		return NULL;

	len = ft_packet_length (packet);
	cmd = ft_packet_command (packet) | ft_packet_flags (packet);

	len = (uint16_t) htons ((unsigned short)len);
	cmd = (uint16_t) htons ((unsigned short)cmd);

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
FTPacket *ft_packet_unserialize (const unsigned char *data, size_t data_len)
{
	FTPacket *packet;
	uint16_t cmd;
	uint16_t len;

	if (data_len < FT_PACKET_HEADER)
		return NULL;

	len = net_get16 ((unsigned char *)data, TRUE);
	cmd = net_get16 ((unsigned char *)(data + 2), TRUE);

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

void ft_packet_set_command (FTPacket *packet, uint16_t cmd)
{
	packet->command &= ~FT_COMMAND_MASK;
	packet->command |= (cmd & FT_COMMAND_MASK);
}

void ft_packet_set_flags (FTPacket *packet, uint16_t flags)
{
	packet->command &= ~FT_FLAGS_MASK;
	packet->command |= (flags & FT_FLAGS_MASK);
}

void ft_packet_set_length (FTPacket *packet, uint16_t len)
{
	if (len > FT_PACKET_MAX)
		return;

	packet->len = len;
}

uint16_t ft_packet_command (FTPacket *packet)
{
	if (!packet)
		return 0;

	return (packet->command & FT_COMMAND_MASK);
}

uint16_t ft_packet_flags (FTPacket *packet)
{
	if (!packet)
		return 0;

	return (packet->command & FT_FLAGS_MASK);
}

uint16_t ft_packet_length (FTPacket *packet)
{
	if (!packet)
		return 0;

	return packet->len;
}

int ft_packet_remaining (FTPacket *packet)
{
	if (!packet)
		return 0;

	assert (packet->len >= packet->offset);
	return (packet->len - packet->offset - packet->overrun);
}

size_t ft_packet_overrun (FTPacket *packet)
{
	if (!packet)
		return 0;

	return packet->overrun;
}

/*****************************************************************************/

static BOOL packet_append (FTPacket *packet, void *data, size_t size)
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

BOOL ft_packet_put_uint (FTPacket *packet, void *data, size_t size,
                         int host_order)
{
	BOOL     ret;
	uint16_t unet16;
	uint32_t unet32;

	if (!data || size < 0 || size > sizeof (uint32_t))
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

BOOL ft_packet_put_uint8 (FTPacket *packet, uint8_t data)
{ return ft_packet_put_uint (packet, &data, sizeof (data), FALSE); }

BOOL ft_packet_put_uint16 (FTPacket *packet, uint16_t data, int host_order)
{ return ft_packet_put_uint (packet, &data, sizeof (data), host_order); }

BOOL ft_packet_put_uint32 (FTPacket *packet, uint32_t data, int host_order)
{ return ft_packet_put_uint (packet, &data, sizeof (data), host_order); }

BOOL ft_packet_put_uarray (FTPacket *packet, int size, void *data,
                           int host_order)
{
	unsigned char *ptr;
	uint32_t       zero = 0;

	if (data)
	{
		for (ptr = data; memcmp (ptr, &zero, size) != 0; ptr += size)
			ft_packet_put_uint (packet, ptr, size, host_order);
	}

	return ft_packet_put_uint (packet, &zero, size, host_order);
}

BOOL ft_packet_put_ip (FTPacket *packet, in_addr_t ip)
{
	/* IPv4 address */
	if (!ft_packet_put_uint16 (packet, 0x4, TRUE))
		return FALSE;

	/* address itself */
	if (!ft_packet_put_uint32 (packet, (uint32_t)ip, FALSE))
		return FALSE;

	return TRUE;
}

BOOL ft_packet_put_ustr (FTPacket *packet, unsigned char *str, size_t len)
{
	static unsigned char nul[64] = { 0 };
	BOOL ret;

	assert (len > 0);

	if (str)
		ret = packet_append (packet, str, len);
	else
	{
		assert (len <= sizeof (nul));
		ret = packet_append (packet, nul, len);
	}

	return ret;
}

BOOL ft_packet_put_str (FTPacket *packet, char *str)
{ return ft_packet_put_ustr (packet, str, gift_strlen (str) + 1); }

BOOL ft_packet_put_raw (FTPacket *packet, unsigned char *str, size_t len)
{ return packet_append (packet, str, len); }

/*****************************************************************************/

static BOOL check_overrun (FTPacket *packet, size_t size)
{
	size_t req;

	assert (packet != NULL);

	req = packet->offset + size;

	if (req > packet->len)
	{
<<<<<<< ft_packet.c
		packet->overrun = packet->offset + size - packet->len;
		packet->offset = packet->len;

=======
		packet->offset = packet->len;
		packet->overrun += (req - packet->len);

>>>>>>> 1.48
		return TRUE;
	}

	return FALSE;
}

uint32_t ft_packet_get_uint (FTPacket *packet, size_t size, int host_order)
{
	uint32_t       data32 = 0;
	unsigned char *p;

	if (!packet)
		return 0;

	assert (size > 0);
	assert (size <= sizeof (uint32_t));

	if (check_overrun (packet, size))
		return 0;

	p = packet->data + FT_PACKET_HEADER + packet->offset;

	switch (size)
	{
	 case 1:
		data32 = (uint32_t)net_get8 (p);
		break;
	 case 2:
		data32 = (uint32_t)net_get16 (p, host_order);
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

uint8_t ft_packet_get_uint8 (FTPacket *packet)
{ return (uint8_t)ft_packet_get_uint (packet, 1, FALSE); }

uint16_t ft_packet_get_uint16 (FTPacket *packet, int host_order)
{ return (uint16_t)ft_packet_get_uint (packet, 2, host_order); }

uint32_t ft_packet_get_uint32 (FTPacket *packet, int host_order)
{ return ft_packet_get_uint (packet, 4, host_order); }

in_addr_t ft_packet_get_ip (FTPacket *packet)
{
	uint16_t ip_ver;
	uint32_t ip;

	if (!(ip_ver = ft_packet_get_uint16 (packet, TRUE)))
		return 0;

	if (ip_ver != 4)
	{
		FT->warn (FT, "invalid ip version: %hu", ip_ver);
		return 0;
	}

	/* the address is already in network order, do not call ntohl... */
	ip = ft_packet_get_uint32 (packet, FALSE);

	return (in_addr_t)ip;
}

static int array_range (FTPacket *packet, size_t size,
                        unsigned char **start, unsigned char **stop)
{
	if (!packet || size == 0)
		return FALSE;

	if (check_overrun (packet, 1))
		return FALSE;

	/* TODO: handle alignment at `size' bounds for stop */
	*start = packet->data + FT_PACKET_HEADER + packet->offset;
	*stop  = packet->data + FT_PACKET_HEADER + packet->len;

	return TRUE;
}

static void array_ho (unsigned char *ptr, size_t size, int host_order)
{
	/* only switch the byte order if we are requesting the data to come out
	 * in host order (OpenFT guarantees all protocol comm will be in network
	 * order) */
	if (!host_order)
		return;

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

void *ft_packet_get_array (FTPacket *packet, size_t size, size_t nmemb,
                           int host_order)
{
	unsigned char *ptr;
	unsigned char *start;
	unsigned char *stop;

	if (!array_range (packet, size, &start, &stop))
		return NULL;

	for (ptr = start; ptr + size <= stop; ptr += size, nmemb--)
	{
		if (nmemb == 0)
			break;

		array_ho (ptr, size, host_order);
	}

	/* not enough data, force the packet offset to completion */
	if (nmemb > 0)
	{
		packet->offset = packet->len;
		return NULL;
	}

	packet->offset += (ptr - start);

	return start;
}

static int array_sentinel (unsigned char *ptr, size_t size)
{
	static unsigned char sent[4] = { 0, 0, 0, 0 };

	return (memcmp (ptr, sent, size) == 0);
}

void *ft_packet_get_arraynul (FTPacket *packet, size_t size, int host_order)
{
	unsigned char *ptr;
	unsigned char *start;
	unsigned char *stop;

	if (!array_range (packet, size, &start, &stop))
		return NULL;

	for (ptr = start; ptr + size <= stop; ptr += size)
	{
		if (array_sentinel (ptr, size))
			break;

		array_ho (ptr, size, host_order);
	}

	if (ptr + size > stop)
	{
		packet->offset = packet->len;
		packet->overrun += size;

		return NULL;
	}

	packet->offset += (ptr - start);
	packet->offset += size;            /* sentinel */

	return start;
}

unsigned char *ft_packet_get_ustr (FTPacket *packet, size_t len)
{ return ft_packet_get_array (packet, 1, len, FALSE); }

char *ft_packet_get_str (FTPacket *packet)
{ return ft_packet_get_arraynul (packet, 1, FALSE); }

unsigned char *ft_packet_get_raw (FTPacket *packet, size_t *retlen)
{
	unsigned char *data;
	size_t         len;

	if (!packet)
		return NULL;

	if (check_overrun (packet, 1))
		return NULL;

	data = packet->data + FT_PACKET_HEADER + packet->offset;
	len  = packet->len - packet->offset;

	if (retlen)
		*retlen = len;

	return data;
}

/*****************************************************************************/

int ft_packet_send (TCPC *c, FTPacket *packet)
{
	unsigned char *data;
	size_t         len = 0;
	int            ret;

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

	if (!(data = ft_packet_serialize (packet, &len)))
		return -1;

	ret = tcp_write (c, data, len);
	ft_packet_free (packet);

	return ret;
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
		ft_netorg_foreach (FT_NODE_SEARCH, FT_NODE_CONNECTED, 0,
		                   FT_NETORG_FOREACH(locate_to), &to);

		/* we haven't delivered anything yet, but may eventually */
		return 0;
	}

	/* send the message directly to this connection */
	return ft_packet_send (FT_CONN(node), packet);
}

/*****************************************************************************/

void put_uint32_array (FTPacket *packet, uint32_t *array)
{
	for (; array && *array; array++)
		ft_packet_put_uint32 (packet, *array, TRUE);

	ft_packet_put_uint32 (packet, 0, TRUE);
}

int ft_packet_sendva (TCPC *c, uint16_t cmd, uint16_t flags,
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
			ft_packet_put_uint32 (packet, (uint32_t)va_arg (args, long), TRUE);
			break;
		 case 'L':
			put_uint32_array (packet, va_arg (args, uint32_t *));
			break;
		 case 'h':
			ft_packet_put_uint16 (packet, (uint16_t)va_arg (args, long), TRUE);
			break;
		 case 'c':
			ft_packet_put_uint8 (packet, (uint8_t)va_arg (args, long));
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
