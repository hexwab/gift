/*
 * $Id: query.c,v 1.10 2004/06/04 15:44:59 hipnod Exp $
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

#include "gt_share.h"
#include "gt_share_file.h"
#include "gt_share_state.h"

#include "gt_search.h"
#include "gt_search_exec.h"
#include "gt_urn.h"

#include "transfer/push_proxy.h"

/*****************************************************************************/

#define LOG_RESULT_PACKETS    gt_config_get_int("search/log_result_packets=0")

/*****************************************************************************/

typedef struct gt_search_reply
{
	uint8_t     ttl;
	uint8_t     results;  /* number of results on the current packet */
	GtPacket   *packet;   /* the current packet to stack results on */
	gt_guid_t  *guid;
} gt_search_reply_t;

/*****************************************************************************/

/* cache of recent queries TODO: flush this on plugin unload */
static Dataset    *query_cache    = NULL;

/* flushes the old cache entries */
static timer_id    flush_timer    = 0;

/*****************************************************************************/

static BOOL is_printable (const char *s)
{
	while (*s)
	{
		if (!isprint (*s))
			return FALSE;

		s++;
	}

	return TRUE;
}

static void parse_text_meta (const char *data, Dataset **meta)
{
	int      rate, freq, min, sec;
	int      n;
	char    *lower;

	if (!data)
		return;

	/* only ASCII strings are plaintext metadata */
	if (!is_printable (data))
		return;

	/* skip strings that start with "urn:", we know what those are */
	if (!strncasecmp (data, "urn:", 4))
		return;

	if (!(lower = STRDUP (data)))
		return;

	string_lower (lower);
	n = sscanf (lower, "%d kbps %d khz %d:%d", &rate, &freq, &min, &sec);

	/* try again with a slightly different format if it failed */
	if (n != 4)
		n = sscanf (lower, "%d kbps(vbr) %d khz %d:%d", &rate, &freq, &min, &sec);

	free (lower);

	if (n != 4)
	{
#if 0
		static int warned = 0;

		if (warned++ < 4)
			GT->DBGFN (GT, "unknown plaintext metadata?: %s", data);
#endif

		return;
	}

	/* XXX: actually this should be META_DEBUG */
	if (XML_DEBUG)
		GT->DBGFN (GT, "parsed %d kbps %d khz %d:%d", rate, freq, min, sec);

	dataset_insertstr (meta, "bitrate",   stringf ("%li", rate * 1000));
	dataset_insertstr (meta, "frequency", stringf ("%u", freq * 1000));
	dataset_insertstr (meta, "duration",  stringf ("%i", min * 60 + sec));
}

void gt_parse_extended_data (char *ext_block, gt_urn_t **r_urn,
                             Dataset **r_meta)
{
	gt_urn_t  *urn = NULL;
	char      *ext;

	if (r_urn)
		*r_urn = NULL;
	if (r_meta)
		*r_meta = NULL;

	if (!ext_block)
		return;

	/*
	 * 0x1c is the separator character for so-called "GEM"
	 * (Gnutella-Extension Mechanism) extensions.
	 */
	while ((ext = string_sep (&ext_block, "\x1c")))
	{
		if (string_isempty (ext))
			break;

		if (r_urn && (urn = gt_urn_parse (ext)))
		{
			free (*r_urn);
			*r_urn = urn;
		}

		if (r_meta)
		{
			parse_text_meta (ext, r_meta);
			gt_xml_parse (ext, r_meta);
		}
	}
}

static BOOL append_result (GtPacket *packet, FileShare *file)
{
	GtShare    *share;
	Hash       *hash;

	if (!(share = share_get_udata (file, GT->name)))
		return FALSE;

	/* search results
	 * format: <index#> <file size> <file name> <extra data(include hash)> */
	gt_packet_put_uint32 (packet, share->index);
	gt_packet_put_uint32 (packet, file->size);
	gt_packet_put_str    (packet, share->filename);

	/*
	 * This is the information that goes "between the nulls" in a
	 * query hit. The first null comes after the filename.
	 *
	 * This is a bit specific and icky. It should be abstracted away.
	 */
	if ((hash = share_get_hash (file, "SHA1")))
	{
		char *sha1;

		assert (hash->len == SHA1_BINSIZE);

		if ((sha1 = sha1_string (hash->data)))
		{
			char  buf[128];
			int   len;

			/* make the hash be uppercase */
			string_upper (sha1);

			len = strlen (sha1);
			assert (len == SHA1_STRLEN);

			snprintf (buf, sizeof (buf) - 1, "urn:sha1:%s", sha1);
			len += strlen ("urn:sha1:");

			gt_packet_put_ustr (packet, buf, len);
			free (sha1);
		}
	}

	/* put the second null there */
	gt_packet_put_uint8 (packet, 0);

	if (gt_packet_error (packet))
	{
		gt_packet_free (packet);
		return FALSE;
	}

	return TRUE;
}

/* add a trailer to the packets */
static void transmit_results (TCPC *c, GtPacket *packet, uint8_t hits)
{
	gt_eqhd1_t eqhd1 = EQHD1_EMPTY;
	gt_eqhd2_t eqhd2 = EQHD2_EMPTY;
	uint8_t   *ggep;
	size_t     ggep_len;

	/* set the push bit as significant */
	eqhd2 |= EQHD2_HAS_PUSH;
	/* set the busy bit as significant */
 	eqhd1 |= EQHD1_HAS_BUSY;

	/*
	 * We shouldnt mark ourselves firewalled if the destination is
	 * a local ip address and ttl == 1. However, for greater TTLs,
	 * there's no knowing if we should mark it or not...
	 */
	if (GT_SELF->firewalled)
		eqhd1 |= EQHD1_PUSH_FLAG;

	if (upload_availability () == 0)
		eqhd2 |= EQHD2_BUSY_FLAG;

	/* add the query hit descriptor
	 * <vendor id> <length> <qhd_data1> <qhd_data2> <private_data> */
	gt_packet_put_ustr   (packet, (const unsigned char *)"GIFT", 4);
	gt_packet_put_uint8  (packet, 2);
	gt_packet_put_uint8  (packet, eqhd1);
	gt_packet_put_uint8  (packet, eqhd2);

	/* append GGEP block (only contains PUSH proxies for now) */
	if (gt_push_proxy_get_ggep_block (&ggep, &ggep_len))
	    gt_packet_put_ustr (packet, ggep, ggep_len);

	/* client identifier */
	gt_packet_put_ustr (packet, GT_SELF_GUID, 16);

	if (gt_packet_error (packet))
	{
		gt_packet_free (packet);
		return;
	}

#if 0
	GT->DBGFN (GT, "packet before twiddling result number: (will twiddle %i)", hits);
	TRACE_MEM (packet->data, packet->len);
#endif

	/* rewind the packet to the search hit count and replace the hitcount
	 * it is the first byte after the header
	 * XXX: this should use a facility of gt_packet */
	packet->data[GNUTELLA_HDR_LEN] = hits;

#if 0
	GT->DBGFN (GT, "packet after twiddling:");
	TRACE_MEM (packet->data, packet->len);
#endif

	if (LOG_RESULT_PACKETS)
		GT->dbg (GT, "transmitting %i", hits);

	/* send the reply along the path to the node that queried us */
	gt_packet_send (c, packet);
	gt_packet_free (packet);
}

static BOOL query_request_result (TCPC *c, FileShare *file,
                                  gt_search_reply_t *reply)
{
	GtPacket *packet;

	if (!file)
	{
		/* send any remaining data */
		if (reply->packet)
			transmit_results (c, reply->packet, reply->results);

		return FALSE;
	}

	packet = reply->packet;

	if (packet)
	{
		/* send the packet if the max results per packet is reached
		 * or the size of the packet is large */
		if (reply->results == 255 || gt_packet_payload_len (packet) > 2000)
		{
			transmit_results (c, packet, reply->results);

			reply->packet  = NULL;
			reply->results = 0;

			/* handle this item again */
			return TRUE;
		}

		if (append_result (packet, file))
			reply->results++;

		return FALSE;
	}

	/* allocate a packet */
	if (!(packet = gt_packet_new (GT_MSG_QUERY_REPLY, reply->ttl, reply->guid)))
	{
		GIFT_ERROR (("mem failure?"));
		return FALSE;
	}

	/* setup the search header */
	gt_packet_put_uint8  (packet, 0);
	gt_packet_put_port   (packet, GT_SELF->gt_port);
	gt_packet_put_ip     (packet, GT_NODE(c)->my_ip);
	gt_packet_put_uint32 (packet, 0); /* speed (kbits) */

	if (gt_packet_error (packet))
	{
		GIFT_ERROR (("failed seting up search result packet"));
		gt_packet_free (packet);
		return FALSE;
	}

	reply->packet = packet;

	/* handle this item again */
	return TRUE;
}

static BOOL query_request_result_free (TCPC *c, FileShare *file,
                                       gt_search_reply_t *reply)
{
	GtShare *share;

	if (!file)
	{
		free (reply->guid);
		free (reply);
		return FALSE;
	}

	/* just a sanity check */
	if (file && !(share = share_get_udata (file, GT->name)))
		return FALSE;

	return FALSE;
}

/* This emulates the old queue interface */
static BOOL send_result (FileShare *file, void **args)
{
	TCPC              *c     = args[0];
	gt_search_reply_t *reply = args[1];

	while (query_request_result (c, file, reply))
		;

	query_request_result_free (c, file, reply);
	return TRUE;
}

static void send_results (TCPC *c, List *results, gt_search_reply_t *reply)
{
	void *args[2];

	args[0] = c;
	args[1] = reply;

	results = list_foreach_remove (results, (ListForeachFunc)send_result, args);
	assert (results == NULL);

	query_request_result (c, NULL, reply);
	query_request_result_free (c, NULL, reply);
}

static int flush_old (ds_data_t *key, ds_data_t *value, time_t *now)
{
	time_t *timestamp = value->data;

	if (*now - *timestamp >= 10 * EMINUTES)
		return DS_CONTINUE | DS_REMOVE;

	return DS_CONTINUE;
}

static BOOL flush_qcache (Dataset *cache)
{
	time_t now = time (NULL);

	assert (query_cache != NULL);
	dataset_foreach_ex (query_cache, DS_FOREACH_EX(flush_old), &now);

	return TRUE;
}

/* TODO: need to break up this file soon to isolate these things */
static BOOL query_cache_lookup (gt_guid_t *guid)
{
	time_t now;

	if (dataset_lookup (query_cache, guid, GT_GUID_LEN))
		return TRUE;

	/* limit the maximum length the query cache can grow */
	if (dataset_length (query_cache) >= 2000)
		return FALSE;

	/* add the guid for catching duplicates next time */
	now = time (NULL);
	dataset_insert (&query_cache, guid, GT_GUID_LEN, &now, sizeof (now));

	if (!flush_timer)
	{
		flush_timer = timer_add (5 * MINUTES, (TimerCallback)flush_qcache,
		                         NULL);
	}

	return FALSE;
}

GT_MSG_HANDLER(gt_msg_query)
{
	char              *query;
	char              *extended;
	gt_guid_t         *guid;
	gt_urn_t          *urn;
	List              *list;
	uint8_t            ttl;
	uint8_t            hops;
	unsigned char     *hash;
	gt_query_flags_t   flags;
	gt_search_type_t   type;
	gt_search_reply_t *reply;

	flags     = gt_packet_get_uint16 (packet);
	query     = gt_packet_get_str    (packet);
	extended  = gt_packet_get_str    (packet);

	guid = gt_packet_guid (packet);

	/*
	 * TODO: node->share_state can be null here, if the node hasn't
	 * successfully handshaked yet.  Should fix this by storing messages until
	 * handshake is complete.
	 */
	if (node->share_state && node->share_state->hidden)
		return;

	/* don't reply if the host is firewalled and we are too */
	if ((flags & QF_HAS_FLAGS) && (flags & QF_ONLY_NON_FW) &&
	    GT_SELF->firewalled)
	{
		return;
	}

	/* don't reply if this is our own search -- TODO: substitute a
	 * full-fledged routing table */
	if (gt_search_find (guid))
	{
		if (MSG_DEBUG)
		{
			GT->dbg (GT, "not searching, own search (guid %s)",
			         gt_guid_str (guid));
		}

		return;
	}

	/* check if we've handled this search already */
	if (query_cache_lookup (guid))
	{
		if (MSG_DEBUG)
			GT->DBGSOCK (GT, c, "duplicate search (%s)", gt_guid_str (guid));

		return;
	}

	gt_parse_extended_data (extended, &urn, NULL);

	/* WARNING: this assumes sha1 */
	hash = gt_urn_data (urn);

	if (hash)
		type = GT_SEARCH_HASH;
	else
		type = GT_SEARCH_KEYWORD;

#if 0
	GT->DBGFN (GT, "min_speed = %hu, query = '%s', extended data = '%s'",
	           min_speed, query, extended);
#endif

	ttl  = gt_packet_ttl  (packet);
	hops = gt_packet_hops (packet);

	list = gt_search_exec (query, type, urn, ttl, hops);
	free (urn);

	if (!list)
		return;

	if (!(reply = MALLOC (sizeof (gt_search_reply_t))))
	{
		list_free (list);
		return;
	}

	/* set the ttl of the reply to be +1 the hops the request travelled */
	reply->ttl = gt_packet_hops (packet) + 1;

	/* use the guid of the packet in replying to results */
	reply->guid = gt_guid_dup (guid);

	send_results (c, list, reply);
}
