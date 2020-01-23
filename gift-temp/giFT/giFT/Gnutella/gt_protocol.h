/*
 * $Id: gt_protocol.h,v 1.3 2003/04/26 20:28:19 hipnod Exp $
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

#ifndef __GT_PROTOCOL_H__
#define __GT_PROTOCOL_H__

typedef enum _gt_packet_type
{
	GT_PING_REQUEST   = 0x00,
	GT_PING_RESPONSE  = 0x01,
	GT_BYE_REQUEST    = 0x02,
	GT_QUERY_ROUTE    = 0x30,
	GT_PUSH_REQUEST   = 0x40,
	GT_QUERY_REQUEST  = 0x80,
	GT_QUERY_RESPONSE = 0x81,
} GtPacketType;

/* 
 * These flags exist in what used to be the MinSpeed field of
 * queries. The documentation for this field is arranged as two
 * bytes in big-endian order, but this uses it in little-endian
 * order so as to be consistent with the rest of the protocol. 
 */
typedef enum gt_query_flags
{
	QF_USE_XML       = 0x0020,    /* servent wants XML metadata */
	QF_FIREWALLED    = 0x0040,    /* source is firewalled */
	QF_HAS_FLAGS     = 0x0080,    /* this query has this interpretation */
} GtQueryFlags;

/* Erm, ugly node state encoded in a query-hit */
typedef enum _gt_qhd_type1
{
	QHD1_PUSH_FLAG   = 0x01,  /* send a push request for this result */
	QHD1_HAS_BAD     = 0x02,  /* bad flag in qhd[2] is signficant,always 0? */
	QHD1_HAS_BUSY    = 0x04,  /* busy ... */
	QHD1_HAS_STABLE  = 0x08,  /* stable ... */
	QHD1_HAS_SPEED   = 0x10,  /* speed ... */
	QHD1_HAS_GGEP    = 0x20,  /* query-hit has GGEP encoded block */
} QueryHitDescriptor1;

typedef enum _gt_qhd_type2
{
	QHD2_HAS_PUSH    = 0x01,  /* set if push flag is significant */
	QHD2_BAD_FLAG    = 0x02,  /* always 0? */
	QHD2_BUSY_FLAG   = 0x04,  /* set if for no availability */
	QHD2_STABLE_FLAG = 0x08,  /* set if transmitted an upload */
	QHD2_SPEED_FLAG  = 0x10,  /* if set, speed is max attained upload speed */
	QHD2_HAS_GGEP    = 0x20,  /* packet has GGEP */
} QueryHitDescriptor2;

void   gnutella_connection     (int fd, input_id id, Connection *c);

#endif /* __GT_PROTOCOL_H__ */
