/*
 * $Id: gt_packet.c,v 1.35 2004/04/17 06:05:53 hipnod Exp $
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

#include "gt_gnutella.h"

#include "gt_packet.h"

#include "gt_node.h"
#include "gt_netorg.h"

#include "gt_utils.h"

#include "gt_query_route.h"
#include "message/gt_message.h"  /* gt_vendor_msg_get_{id,version} */

#include <stdarg.h>

/*****************************************************************************/

GtPacket *gt_packet_new (uint8_t cmd, uint8_t ttl, gt_guid_t *guid)
{
	GtPacket      *packet;
	unsigned char *data;

	if (!(packet = MALLOC (sizeof (GtPacket))))
		return NULL;

	if (!(data = MALLOC (GT_PACKET_INITIAL)))
	{
		free (packet);
		return NULL;
	}

	packet->data     = data;
	packet->len      = GNUTELLA_HDR_LEN;
	packet->data_len = GT_PACKET_INITIAL;

	/*
	 * Caller could provide GUID or request it be randomly initialized.
	 */
	if (guid)
		gt_packet_set_guid (packet, guid);
	else
		gt_guid_init ((gt_guid_t *)packet->data);

	gt_packet_set_command     (packet, cmd);
	gt_packet_set_ttl         (packet, ttl);
	gt_packet_set_payload_len (packet, 0);
	gt_packet_set_hops        (packet, 0);

	/* set the offset to start at the end of the header */
	packet->offset = GNUTELLA_HDR_LEN;

	return packet;
}

GtPacket *gt_packet_reply (GtPacket *src, uint8_t cmd)
{
	GtPacket  *packet;
	uint8_t    hops;
	gt_guid_t *guid;

	hops = gt_packet_hops (src);
	guid = gt_packet_guid (src);

	if (!(packet = gt_packet_new (cmd, hops + 1, guid)))
		return NULL;

	return packet;
}

GtPacket *gt_packet_vendor (const gt_vendor_msg_t *vmsg)
{
	GtPacket            *pkt;

	if (!(pkt = gt_packet_new (GT_MSG_VENDOR, 1, NULL)))
		return NULL;

	gt_packet_put_ustr   (pkt, vmsg->vendor_id, 4);
	gt_packet_put_uint16 (pkt, vmsg->id);
	gt_packet_put_uint16 (pkt, 1); /* vmsg version */

	if (gt_packet_error (pkt))
	{
		gt_packet_free (pkt);
		return NULL;
	}

	return pkt;
}

void gt_packet_free (GtPacket *packet)
{
	if (!packet)
		return;

	free (packet->data);
	free (packet);
}

/*****************************************************************************/

gt_guid_t *gt_packet_guid (GtPacket *packet)
{
	assert (packet->data_len >= GNUTELLA_HDR_LEN);

	return packet->data;
}

uint32_t gt_packet_payload_len (GtPacket *packet)
{
	assert (packet->data_len >= GNUTELLA_HDR_LEN);

	return get_payload_len (packet->data);
}

uint32_t gt_packet_command (GtPacket *packet)
{
	assert (packet->data_len >= GNUTELLA_HDR_LEN);

	return get_command (packet->data);
}

uint8_t gt_packet_hops (GtPacket *packet)
{
	assert (packet->data_len >= GNUTELLA_HDR_LEN);

	return get_hops (packet->data);
}

uint8_t gt_packet_ttl (GtPacket *packet)
{
	assert (packet->data_len >= GNUTELLA_HDR_LEN);

	return get_ttl (packet->data);
}

/*****************************************************************************/

void gt_packet_set_guid (GtPacket *packet, gt_guid_t *guid)
{
	assert (packet->data_len >= GNUTELLA_HDR_LEN);

	memcpy (packet->data, guid, 16);
}

void gt_packet_set_command (GtPacket *packet, uint8_t cmd)
{
	assert (packet->data_len >= GNUTELLA_HDR_LEN);

	get_command (packet->data) = cmd;
}

void gt_packet_set_ttl (GtPacket *packet, uint8_t ttl)
{
	assert (packet->data_len >= GNUTELLA_HDR_LEN);

	get_ttl (packet->data) = ttl;
}

void gt_packet_set_hops (GtPacket *packet, uint8_t hops)
{
	assert (packet->data_len >= GNUTELLA_HDR_LEN);

	get_hops (packet->data) = hops;
}

void gt_packet_set_payload_len (GtPacket *packet, uint32_t len)
{
	assert (packet->data_len >= GNUTELLA_HDR_LEN);

	len = htovl (len);

	memcpy (packet->data + 19, &len, 4);
}

/*****************************************************************************/
/* functions to read data from packets */

static uint8_t gt_get8 (unsigned char *src)
{
	return *src;
}

#define copy_bytes(src,sz,endian,swap,sizedvar_t,swap_net,swap_vax) \
	sizedvar_t r;                        \
	sizedvar_t v;                        \
	sizedvar_t n;                        \
	                                     \
	memcpy (&r, src, sz);                \
	                                     \
	n = swap_net (r);                    \
	                                     \
	assert_at_compile (BIG == 0x01);     \
	if (endian & swap)                   \
	    r = n;                           \
	                                     \
	v = swap_vax (r);                    \
	                                     \
	assert_at_compile (LITTLE == 0x00);  \
	if (~endian & swap)                  \
	    r = v;                           \
	                                     \
	return r;

static uint16_t gt_get16 (unsigned char *src, int endian, int swap)
{
	copy_bytes (src, 2, endian, swap, uint16_t, ntohs, vtohs);
}

static uint32_t gt_get32 (unsigned char *src, int endian, int swap)
{
	copy_bytes (src, 4, endian, swap, uint32_t, ntohl, vtohl);
}

/*****************************************************************************/

static BOOL gt_packet_resize (GtPacket *packet, size_t len)
{
	size_t         resize_len;
	unsigned char *resized;

	if (!packet)
		return FALSE;

	assert (len >= GNUTELLA_HDR_LEN);
	assert (len < GT_PACKET_MAX);

	/* the buffer we have allocated is already large enough */
	if (packet->data_len >= len)
		return TRUE;

	/* determine an appropriate resize length */
	for (resize_len = packet->data_len; resize_len < len; )
	{
		if (resize_len == 0)
			resize_len = GT_PACKET_INITIAL;
		else
			resize_len *= 2;
	}

	if (!(resized = realloc (packet->data, resize_len)))
	{
		packet->error = TRUE;
		return FALSE;
	}

	memset (resized + packet->data_len, 0, resize_len - packet->data_len);

	/* modify the packet structure to reflect this resize */
	packet->data_len = resize_len;
	packet->data     = resized;

	return TRUE;
}

int gt_packet_error (GtPacket *packet)
{
	return packet->error;
}

int gt_packet_seek (GtPacket *packet, int offset)
{
	int old_offset;

	/* should we be setting packet->error here? */
	if (offset >= packet->len || offset < 0)
	{
		packet->error = TRUE;
		return -1;
	}

	old_offset = packet->offset;
	packet->offset = offset;

	return old_offset;
}

GtPacket *gt_packet_unserialize (unsigned char *data, size_t len)
{
	GtPacket *packet;

	if (!(packet = gt_packet_new (0, 0, NULL)))
		return NULL;

	if (len >= GT_PACKET_MAX)
	{
		gt_packet_free (packet);
		return NULL;
	}

	if (!gt_packet_resize (packet, len))
	{
		GIFT_ERROR (("error resizing packet"));
		gt_packet_free (packet);
		return NULL;
	}

	memcpy (packet->data, data, len);

	packet->len = len;

	/* Hmm, this should never happen, time to valgrind */
	if (gt_packet_payload_len (packet) != len - GNUTELLA_HDR_LEN)
	{
		GIFT_ERROR (("corrupt packet"));
		gt_packet_free (packet);
		return NULL;
	}

	return packet;
}

/*****************************************************************************/

uint8_t gt_packet_get_uint8 (GtPacket *packet)
{ return gt_packet_get_uint (packet, 1, LITTLE, FALSE); }

uint16_t gt_packet_get_uint16 (GtPacket *packet)
{ return gt_packet_get_uint (packet, 2, LITTLE, TRUE); }

uint32_t gt_packet_get_uint32 (GtPacket *packet)
{ return gt_packet_get_uint (packet, 4, LITTLE, TRUE); }

in_addr_t gt_packet_get_ip (GtPacket *packet)
{ return gt_packet_get_uint (packet, 4, BIG, FALSE); }

in_port_t gt_packet_get_port (GtPacket *packet)
{ return gt_packet_get_uint (packet, 2, LITTLE, TRUE); }

/*****************************************************************************/

uint32_t gt_packet_get_uint (GtPacket *packet, size_t size,
                              int endian, int swap)
{
	uint32_t    data32 = 0;
	char       *offs;

	assert (packet);

	/* check overrun */
	if (packet->offset + size > packet->len)
	{
		packet->error = TRUE;
		return 0;
	}

	offs = packet->data + packet->offset;

	switch (size)
	{
	 case 1:
		data32 = (uint32_t)gt_get8 (offs);
		break;
	 case 2:
		data32 = (uint32_t)gt_get16 (offs, endian, swap);
		break;
	 case 4:
		data32 = gt_get32 (offs, endian, swap);
		break;
	 default:
		printf ("%s: wtf are you doing?\n", __PRETTY_FUNCTION__);
		return data32;
	}

	packet->offset += size;

	return data32;
}

/*****************************************************************************/

static int gt_packet_append (GtPacket *packet, const void *data, size_t size)
{
	size_t required;

	if (!packet || !data || size == 0)
		return FALSE;

	if (packet->data_len + size >= GT_PACKET_MAX)
	{
		packet->error = TRUE;
		return FALSE;
	}

	/* determine the total required space to append size bytes */
	required = packet->len + size;

	/* if we can't resize, gracefully fail...the caller should determine
	 * how bad this truly is */
	if (!gt_packet_resize (packet, required))
		return FALSE;

	/* append size bytes of data */
	memcpy (packet->data + packet->len, data, size);

	/* increment the length of this packet */
	packet->len += size;

	gt_packet_set_payload_len (packet, gt_packet_payload_len (packet) + size);
	return TRUE;
}

int gt_packet_put_uint (GtPacket *packet, void *data, size_t size,
                        int endian, int swap)
{
	int ret;
	uint16_t unet16;
	uint32_t unet32;

	if (!data || size < 0 || size > sizeof (uint32_t))
		return FALSE;

	switch (size)
	{
	 case 2:
		unet16 = gt_get16 (data, endian, swap);
		ret = gt_packet_append (packet, &unet16, size);
		break;
	 case 4:
		unet32 = gt_get32 (data, endian, swap);
		ret = gt_packet_append (packet, &unet32, size);
		break;
	 default:
		ret = gt_packet_append (packet, data, size);
		break;
	}

	return ret;
}

/*****************************************************************************/

int gt_packet_put_uint8 (GtPacket *packet, uint8_t byte)
{ return gt_packet_put_uint (packet, &byte, 1, LITTLE, FALSE); }

int gt_packet_put_uint16 (GtPacket *packet, uint16_t bytes)
{ return gt_packet_put_uint (packet, &bytes, 2, LITTLE, TRUE); }

int gt_packet_put_uint32 (GtPacket *packet, uint32_t bytes)
{ return gt_packet_put_uint (packet, &bytes, 4, LITTLE, TRUE); }

int gt_packet_put_ip (GtPacket *packet, in_addr_t ip)
{ return gt_packet_put_uint (packet, &ip, 4, BIG, FALSE); }

int gt_packet_put_port (GtPacket *packet, in_port_t port)
{ return gt_packet_put_uint (packet, &port, 2, LITTLE, TRUE); }

int gt_packet_put_ustr (GtPacket *packet, const unsigned char *str, size_t len)
{
	assert (len > 0);
	return gt_packet_append (packet, str, len);
}

int gt_packet_put_str (GtPacket *packet, const char *str)
{
	size_t len;

	if (!str)
		return gt_packet_put_uint8 (packet, 0);

	len = strlen (str) + 1;
	return gt_packet_put_ustr (packet, (const unsigned char *)str, len);
}

/*****************************************************************************/

/* checks for the gt_packet_get_array sentinel given the size.  the sentinel is
 * defined as a full element filled with zeroes */
static int array_sentinel (char *ptr, size_t size)
{
	while (size--)
	{
		/* non-zero byte, this element couldnt be the sentinel */
		if (*ptr != 0)
			return FALSE;

		ptr++;
	}

	return TRUE;
}

void *gt_packet_get_array (GtPacket *packet, size_t nmemb, size_t size,
                           int term, int endian, int swap)
{
	char *start;
	char *ptr;
	char *end;
	int   n;

	assert (packet);

	/* we tried to read past the end of the packet */
	if (packet->offset >= packet->len)
	{
		packet->error = TRUE;
		return NULL;
	}

	start = packet->data + packet->offset;
	end   = packet->data + packet->len;

	/* TODO - optimize compares inside this loop */
	n = 0;
	for (ptr = start; ptr + size < end; ptr += size, n++)
	{
		if (term && array_sentinel (ptr, size))
			break;

		if (nmemb > 0 && n >= nmemb)
			break;

		if (swap)
		{
			switch (size)
			{
			 case 2:
				net_put16 (ptr, gt_get16 (ptr, endian, swap));
				break;
			 case 4:
				net_put32 (ptr, gt_get32 (ptr, endian, swap));
				break;
			 default:
				assert (0);
				break;
			}
		}
	}

	/* If the array was not null terminated, then terminate it now */
	if (term && !array_sentinel (ptr, size))
	{
		uint32_t   zero = 0;
		size_t     len;

		/* this is the length of the array we read */
		len = (ptr - start + size);

		/* we must have hit the end of the packet */
		assert (packet->offset + len == packet->len);

		if (!gt_packet_resize (packet, packet->len + size))
		{
			packet->offset = packet->len;
			return NULL;
		}

		/* Hrm, this changes the payload, which we really dont want to do. */
		if (!gt_packet_append (packet, &zero, size))
		{
			packet->offset = packet->len;
			return NULL;
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
	if (term)
		ptr += size;

	/* this includes the sentinel now */
	packet->offset += (ptr - start);

	return start;
}

char *gt_packet_get_str (GtPacket *packet)
{
	return gt_packet_get_array (packet, 0, 1, TRUE, FALSE, FALSE);
}

unsigned char *gt_packet_get_ustr (GtPacket *packet, size_t len)
{
	return gt_packet_get_array (packet, len, 1, FALSE, FALSE, FALSE);
}

/******************************************************************************/

static char *packet_command_str (unsigned char cmd)
{
	static char buf[16];

	switch (cmd)
	{
	 case GT_MSG_PING:        return "PING";
	 case GT_MSG_PING_REPLY:  return "PONG";
	 case GT_MSG_BYE:         return "BYE";
	 case GT_MSG_QUERY_ROUTE: return "QROUTE";
	 case GT_MSG_VENDOR:      return "VMSG";
	 case GT_MSG_VENDOR_STD:  return "VMSG-S";
	 case GT_MSG_PUSH:        return "PUSH";
	 case GT_MSG_QUERY:       return "QUERY";
	 case GT_MSG_QUERY_REPLY: return "HITS";

	 default:
		snprintf (buf, sizeof (buf), "[<%02hx>]", cmd);
		return buf;
	}
}

static void packet_log (char *data, int len, int sent, char *user_agent,
                        in_addr_t ip)
{
	uint8_t      cmd;
	char         user_buf[32];
	char        *file;
	static FILE *ascii_log;

	if (!ascii_log)
	{
		file = PACKET_ASCII_LOG;

		if (!(ascii_log = fopen (file, "w")))
			return;
	}

	cmd = get_command (data);

	user_buf[0] = 0;

	/* copy the first few chars of user_agent to make it line up */
	if (user_agent)
	{
		strncpy (user_buf, user_agent, 21);
		user_buf[21] = 0;
	}

	fprintf (ascii_log, "%2s %-6s sz: %-5hu peer: %-22s [%s]\n",
	         (sent ? "<=" : "=>"),
	         packet_command_str (cmd), len,
	         (user_buf[0] ? user_buf : "(None)"),
	         (ip == 0 ? "None" : net_ip_str (ip)));

	fprint_hex (ascii_log, data, len);
}

void gt_packet_log (GtPacket *packet, TCPC *src, int sent)
{
	char      *user_agent;
	in_addr_t  ip;

	if (!PACKET_DEBUG)
		return;

	user_agent = NULL;
	ip         = 0;

	/* append the user-agent string to the packet log */
	if (src)
	{
		ip         = src->host;
		user_agent = dataset_lookupstr (GT_NODE(src)->hdr, "user-agent");
	}

	packet_log (packet->data, packet->len, sent, user_agent, ip);
}

/******************************************************************************/

int gt_packet_send (TCPC *c, GtPacket *packet)
{
	if (!c || c->fd < 0)
		return -1;

	if (!gt_node_send (GT_NODE(c), packet))
		return -1;

	return 0;
}

/******************************************************************************/

static int send_packetva (TCPC *c, uint8_t cmd,
                          gt_guid_t *guid, uint8_t ttl,
                          uint8_t hops, char *fmt, va_list args)
{
	GtPacket *pkt;
	char      *p;
	int        short_fmt   = FALSE;
	int        field_width = 0;
	int        ret;

	if (!(pkt = gt_packet_new (cmd, ttl, guid)))
		return -1;

	for (p = fmt; *p; p++)
	{
		switch (*p)
		{
		 case '%':
			short_fmt = FALSE;
			break;
		 case 'h':
			short_fmt = TRUE;
			break;
		 case 'l':
			break;
		 case 'c':
			{
				uint8_t bits8 = (uint8_t) va_arg (args, int);
				gt_packet_put_uint8 (pkt, bits8);
			}
			break;
		 case 'u':
			if (short_fmt)
			{
				uint16_t bits16 = (uint16_t) va_arg (args, int);
				gt_packet_put_uint16 (pkt, bits16);
			}
			else
			{
				uint32_t bits32 = (uint32_t) va_arg (args, int);
				gt_packet_put_uint32 (pkt, bits32);
			}
			break;
		 case 's':
			{
				char *str = va_arg (args, char *);
				gt_packet_put_str (pkt, str);
			}
			break;
		 case '0': case '1': case '2': case '3': case '4': case '5':
		 case '6': case '7': case '8': case '9':
			field_width = field_width * 10 + (*p - '0');
			break;
		 case '*':
			field_width = va_arg (args, int);
			break;
		 case 'p':
			{
				unsigned char *ustr = va_arg (args, unsigned char *);

				gt_packet_put_ustr (pkt, ustr, field_width);
				field_width = 0;
			}
			break;
		 default:
			abort ();
			break;
		}
	}

	if (gt_packet_error (pkt))
	{
		gt_packet_free (pkt);
		return -1;
	}

	ret = gt_packet_send (c, pkt);
	gt_packet_free (pkt);

	return ret;
}

int gt_packet_send_fmt (TCPC *c, uint8_t cmd,
                        gt_guid_t *guid, uint8_t ttl,
                        uint8_t hops, char *fmt, ...)
{
	va_list args;
	int     ret;

	va_start (args, fmt);
	ret = send_packetva (c, cmd, guid, ttl, hops, fmt, args);
	va_end (args);

	return ret;
}

int gt_packet_reply_fmt (TCPC *c, GtPacket *packet,
                         uint8_t cmd, char *fmt, ...)
{
	va_list     args;
	int         ret;
	size_t      hops;
	gt_guid_t  *guid;

	guid = gt_packet_guid (packet);
	hops = gt_packet_hops (packet);

	va_start (args, fmt);
	ret = send_packetva (c, cmd, guid, hops + 1, 0, fmt, args);
	va_end (args);

	return ret;
}

/******************************************************************************/

int gt_packet_forward (GtPacket *packet, TCPC *c)
{
	return -1;
}
