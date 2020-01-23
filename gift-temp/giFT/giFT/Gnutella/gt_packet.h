/*
 * $Id: gt_packet.h,v 1.7 2003/04/26 20:03:49 hipnod Exp $
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

#ifndef __GT_PACKET_H__
#define __GT_PACKET_H__

/*****************************************************************************/

#define GNUTELLA_HDR_LEN    23

#define GT_PACKET_INITIAL   (48)
#define GT_PACKET_MAX       (65535)

/*****************************************************************************/

struct _gt_packet
{
	uint16_t       offset;             /* Pointer within packet */
	uint16_t       len;                /* size of packet structure */
	uint16_t       data_len;           /* how big the packet data is */
	uint16_t       error;              /* packet encountered memory error */
	int            ref;                /* ref counter for this packet */

	unsigned char *data;    /* packet header + arbtrary packet data */
};

typedef struct _gt_packet Gt_Packet;

/*****************************************************************************/

#define UNSIGNED(pkt)       ((uint8_t *)(pkt))

#define get_guid(pkt)    ((pkt)[0])
#define get_command(pkt) ((pkt)[16])
#define get_ttl(pkt)     ((pkt)[17])
#define get_hops(pkt)    ((pkt)[18])

/* yuck
 * this shouldnt need to be unsigned, because its left-shifting,
 * but I remember it breaking otherwise...
 * probably because nb->data is not unsigned */
#define get_payload_len(pkt) \
   	((UNSIGNED((pkt))[19]) | (UNSIGNED((pkt))[20] << 8) | \
     (UNSIGNED((pkt))[21] << 16) | (UNSIGNED((pkt))[22] << 24))

/*****************************************************************************/

/* handle little-endian ordering on the network */

#ifdef WORDS_BIGENDIAN
/* argh, watch out for sign extension */
#define vtohl(x)                                           \
    ({                                                     \
        uint32_t _tmp = (x);                              \
        (((_tmp & 0xff) << 24)  | ((_tmp & 0xff00) << 8) | \
        ((_tmp & 0xff0000) >> 8) | (_tmp >> 24));          \
    })

#define vtohs(x)                                         \
    ({                                                   \
        uint16_t _tmp = (x);                            \
        (((_tmp & 0xff) << 8) | ((_tmp >> 8) & 0xff));   \
    })
#else
#define vtohs(x)    (x)
#define vtohl(x)    (x)
#endif /* WORDS_BIGENDIAN */

#define htovs(x)    vtohs (x)
#define htovl(x)    vtohl (x)

/* endianness defines */
#define BIG         1
#define LITTLE      0

/*****************************************************************************/

#ifdef __GNUC__
#define FORMAT_ATTRIBUTE(kind, fmt_arg, first_arg) \
	__attribute__ ((format (kind, fmt_arg, first_arg)))
#else
#define FORMAT_ATTRIBUTE(kind, fmt_arg, first_arg)
#endif

/*****************************************************************************/

Gt_Packet      *gt_packet_new       (uint8_t cmd, uint8_t ttl, gt_guid *guid);
Gt_Packet      *gt_packet_reply     (Gt_Packet *packet, uint8_t cmd);
void            gt_packet_free      (Gt_Packet *packet);
int             gt_packet_error     (Gt_Packet *packet);

Gt_Packet      *gt_packet_unserialize (unsigned char *data, int len);

/*****************************************************************************/

gt_guid        *gt_packet_guid         (Gt_Packet *packet);
uint32_t        gt_packet_payload_len  (Gt_Packet *packet);
uint32_t        gt_packet_command      (Gt_Packet *packet);
uint8_t         gt_packet_hops         (Gt_Packet *packet);
uint8_t         gt_packet_ttl          (Gt_Packet *packet);

/*****************************************************************************/

void            gt_packet_set_guid         (Gt_Packet *packet, gt_guid *guid);
void            gt_packet_set_payload_len  (Gt_Packet *packet, uint32_t len);
void            gt_packet_set_command      (Gt_Packet *packet, uint8_t cmd);
void            gt_packet_set_hops         (Gt_Packet *packet, uint8_t hops);
void            gt_packet_set_ttl          (Gt_Packet *packet, uint8_t ttl);

/*****************************************************************************/

uint32_t        gt_packet_get_uint   (Gt_Packet *packet, size_t size,
                                      int endian, int swap);

uint8_t         gt_packet_get_uint8  (Gt_Packet *packet);
uint16_t        gt_packet_get_uint16 (Gt_Packet *packet);
uint32_t        gt_packet_get_uint32 (Gt_Packet *packet);

in_addr_t       gt_packet_get_ip     (Gt_Packet *packet);
in_port_t       gt_packet_get_port   (Gt_Packet *packet);

char           *gt_packet_get_str    (Gt_Packet *packet);
unsigned char  *gt_packet_get_ustr   (Gt_Packet *packet, int len);

/*****************************************************************************/

int         gt_packet_put_uint8  (Gt_Packet *packet, uint8_t byte);
int         gt_packet_put_uint16 (Gt_Packet *packet, uint16_t bytes);
int         gt_packet_put_uint32 (Gt_Packet *packet, uint32_t bytes);

int         gt_packet_put_str    (Gt_Packet *packet, char *str);
int         gt_packet_put_ustr   (Gt_Packet *packet, unsigned char *ustr,
                                  int len);

int         gt_packet_put_ip     (Gt_Packet *packet, in_addr_t ip);
int         gt_packet_put_port   (Gt_Packet *packet, in_port_t port);

/*****************************************************************************/

int         gt_packet_send       (Connection *c, Gt_Packet *packet);

int         gt_packet_send_fmt   (Connection *c, uint8_t cmd,
                                  gt_guid *guid, uint8_t ttl,
                                  uint8_t hops, char *fmt, ...)
                                    FORMAT_ATTRIBUTE (printf, 6, 7);

int         gt_packet_reply_fmt  (Connection *c, Gt_Packet *packet,
                                  uint8_t cmd, char *fmt, ...)
                                    FORMAT_ATTRIBUTE (printf, 4, 5);


/*****************************************************************************/

void gt_packet_log (Gt_Packet *packet, struct tcp_conn *src, int sent);

/*****************************************************************************/

/* hrm */
void gt_packet_get_ref (Gt_Packet *packet);
void gt_packet_put_ref (Gt_Packet *packet);

/*****************************************************************************/

#endif /* __GT_PACKET_H__ */
