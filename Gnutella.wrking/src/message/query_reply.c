/*
 * $Id: query_reply.c,v 1.3 2004/03/24 06:35:10 hipnod Exp $
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
#include "message/msg_handler.h"

#include "sha1.h"
#include "xml.h"

#include "gt_search.h"

#include "gt_urn.h"
#include "gt_ban.h"

#include "gt_share_file.h"

#include <libgift/mime.h>

/*****************************************************************************/

/*
 * Whether to attach the number of hops each result travelled as a metadata
 * field "Hops". Interesting to see but probably not too useful for the
 * average user, and so disabled by default.
 */
#define HOPS_AS_META         gt_config_get_int("search/hops_as_meta=0")

/*****************************************************************************/

extern void gt_parse_extended_data (char *ext_block, gt_urn_t **r_urn,
                                    Dataset **r_meta); /* query.c */

/*****************************************************************************/

static void add_meta (ds_data_t *key, ds_data_t *value, FileShare *file)
{
	char *keystr   = key->data;
	char *valuestr = value->data;

	share_set_meta (file, keystr, valuestr);
}

/* parse XML data right before the client guid */
static void parse_xml_block (GtPacket *packet, size_t xml_bin_len,
                             Share **results, size_t hits)
{
	int      old_offset;
	char    *xml;
	uint8_t  savechr;

	/* if the length is one there is only the null terminator */
	if (xml_bin_len < 1)
		return;

	/*
	 * Look for the XML before the client guid. Subtract the length of the
	 * client guid and the size of the XML.
	 */
	old_offset = gt_packet_seek (packet, packet->len - 16 - xml_bin_len);

	/* hopefully, if xml_bin_len is bogus this will fail */
	if (old_offset < 0)
		return;

	xml = gt_packet_get_ustr (packet, xml_bin_len);

	if (!xml)
		return;

	/*
	 * Null terminate the packet even if the remote end didn't.  We know this
	 * is safe because the client GUID resides after the XML, and is 16 bytes
	 * long.
	 *
	 * Note that the null char should be at xml[xml_bin_len - 1], so we end up
	 * actually null-terminating twice if the XML is already terminated.
	 * gt_xml_parse_indexed must take this into account (and it does by
	 * largely ignoring xml_bin_len when the XML is plaintext).
	 */
	savechr = xml[xml_bin_len];
	xml[xml_bin_len] = 0;

	if (XML_DEBUG)
		GT->dbg (GT, "xmldata=%s", xml);

	/*
	 * The XML before the client guid that we are parsing has a special
	 * property "index" indicating which share the XML property corresponds
	 * to, and so needs to be handled differently from XML appearing in
	 * each individual hit.
	 */
	gt_xml_parse_indexed (xml, xml_bin_len, results, hits);

	/* put the saved character back */
	xml[xml_bin_len] = savechr;
}

/*
 * Attach the travelled hops as a metadata field.
 */
static void attach_hops (Share *share, int hops)
{
	char buf[12];

	if (!HOPS_AS_META)
		return;

	snprintf (buf, sizeof (buf) - 1, "%u", hops);
	share_set_meta (share, "Hops", buf);
}

void gt_query_hits_parse (GtPacket *packet, GtSearch *search,
                          TCPC *c, gt_guid_t *client_guid)
{
	uint8_t      count;
	in_port_t    port;
	in_addr_t    host;
	uint32_t     speed;
	Share       *results[255];
	uint16_t     xml_len         = 0;
	int          i, availability = 1;
	BOOL         firewalled      = FALSE;
	int          total;

	count = gt_packet_get_uint8  (packet);
	port  = gt_packet_get_port   (packet);
	host  = gt_packet_get_ip     (packet);
	speed = gt_packet_get_uint32 (packet);

	/* check if this host is banned */
	if (gt_ban_ipv4_is_banned (host))
	{
		GT->dbg (GT, "discarding search results from %s [address banned]",
		         net_ip_str (host));
		return;
	}

	for (i = 0; i < count; i++)
	{
		uint32_t       index;
		uint32_t       size;
		char          *fname, *data;
		gt_urn_t      *urn  = NULL;
		Dataset       *meta = NULL;
		Share         *file;

		index = gt_packet_get_uint32 (packet);
		size  = gt_packet_get_uint32 (packet);
		fname = gt_packet_get_str    (packet);
		data  = gt_packet_get_str    (packet);

		/* If there was an error parsing the packet (not enough results),
		 * stop parsing */
		if (gt_packet_error (packet))
			break;

		if (!fname || string_isempty (fname))
		{
			results[i] = NULL;
			continue;
		}

		gt_parse_extended_data (data, &urn, &meta);

		/*
		 * WARNING: calling gt_urn_data here assumes sha1.
		 *
		 * TODO: this is a potential bug if gt_share_new() makes assumptions
		 * about the hash data's size and gt_urn_t changes to be multiple
		 * sizes later.
		 */
		if (!(file = gt_share_new (fname, index, size, gt_urn_data (urn))))
		{
			GIFT_ERROR (("error making fileshare, why?"));

			dataset_clear (meta);
			free (urn);

			/* make sure we find out about it if we're missing results ;) */
			assert (0);

			results[i] = NULL;
			continue;
		}

		/* HACK: set the mimetype from the file extension */
		share_set_mime (file, mime_type (fname));

		dataset_foreach (meta, DS_FOREACH(add_meta), file);

		/* Attach the hops the search travelled as a metadata field */
		attach_hops (file, gt_packet_hops (packet));

		dataset_clear (meta);
		free (urn);

		results[i] = file;
	}

	total = i;

	/* look for the query hit descriptor */
	if (!gt_packet_error (packet) &&
	    packet->len - packet->offset >= 16 + 7 /* min qhd len */)
	{
		unsigned char *vendor;
		uint8_t        eqhd_len;
		uint8_t        eqhd[2];

		vendor   = gt_packet_get_ustr  (packet, 4);
		eqhd_len = gt_packet_get_uint8 (packet);
		eqhd[0]  = gt_packet_get_uint8 (packet);
		eqhd[1]  = gt_packet_get_uint8 (packet);

		/* set availability to 0 or 1 depending on the busy flag */
		availability = ((eqhd[0] & EQHD1_HAS_BUSY) &&
		                !(eqhd[1] & EQHD2_BUSY_FLAG)) ? 1 : 0;

		/* set firewalled status based on the PUSH flag */
		firewalled   = BOOL_EXPR ((eqhd[0] & EQHD1_PUSH_FLAG) &&
		                          (eqhd[1] & EQHD2_HAS_PUSH));

		/*
		 * Check for an XML metadata block, that is usually present
		 * when the size of the "public area" is 4
		 */
		if (eqhd_len >= 4)
			xml_len = gt_packet_get_uint16 (packet);

		if (xml_len > 0)
		{
			if (XML_DEBUG)
			{
				char str[5] = { 0 };

				if (vendor)
					memcpy (str, vendor, 4);

				GT->dbg (GT, "(%s) xml_len=%d", str, xml_len);
			}

			parse_xml_block (packet, xml_len, results, total);
		}

#if 0
		if (MSG_DEBUG)
		{
			GT->DBGFN (GT, "vendor = %s, qhd_len = %u, qhd_0 = %x, qhd_1 = %x,"
			           " availability = %i, firewalled = %i",
			           make_str (vendor, 4), qhd_len, qhd[0], qhd[1],
			           availability, firewalled);
		}
#endif
	}

	/* send the results to the interface protocol */
	for (i = 0; i < total; i++)
	{
		if (results[i])
		{
			gt_search_reply (search, c, host, port, client_guid, availability,
			                 firewalled, results[i]);

			gt_share_unref (results[i]);
		}
	}
}

/* should split this up into two routines */
GT_MSG_HANDLER(gt_msg_query_reply)
{
	GtSearch   *search;
	int         save_offset;
	gt_guid_t  *client_guid;

	/* Each client has a unique identifier at the end of the
	 * packet.  Grab that first. */
	if (packet->len < 16)
	{
		if (MSG_DEBUG)
			GT->DBGSOCK (GT, c, "illegal query response packet, < 16 bytes");

		return;
	}

	/* hack the offset in the packet */
	save_offset = packet->offset;
	packet->offset = packet->len - 16;

	client_guid = gt_packet_get_ustr (packet, 16);

	/* put the offset back */
	packet->offset = save_offset;

	if (!(search = gt_search_find (gt_packet_guid (packet)))
		/*&& query_cache_lookup (packet->guid)*/)
	{
		/* TODO: support forwarding of query responses by
		 * looking up their destinations in the guid cache */

		/*gt_route_forward_packet (packet, c);*/

		/* add the client GUID to the push cache: in case of a
		 * push request we know where to send it */
		/*push_cache_add (client_guid, c);*/

		return;
	}

	gt_query_hits_parse (packet, search, c, client_guid);
}
