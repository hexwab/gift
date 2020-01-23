/*
 * $Id: gt_packet.h,v 1.27 2004/05/02 10:09:41 hipnod Exp $
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

#ifndef GIFT_GT_PACKET_H_
#define GIFT_GT_PACKET_H_

/*****************************************************************************/

#define PACKET_DEBUG         gt_config_get_int("packet/debug=0")
#define PACKET_ASCII_LOG     gt_config_get_str("packet/ascii_log_file=/dev/tty")

/*****************************************************************************/

#define GNUTELLA_HDR_LEN    23

#define GT_PACKET_INITIAL   (48)
#define GT_PACKET_MAX       (65536)    /* smallest invalid, including header */

/*****************************************************************************/

struct gt_packet
{
	uint32_t       offset;             /* Pointer within packet */
	uint32_t       len;                /* size of packet structure */
	uint32_t       data_len;           /* how big the packet data is */
	uint32_t       error;              /* packet encountered memory error */

	unsigned char *data;    /* packet header + arbtrary packet data */
};

typedef struct gt_packet GtPacket;

typedef enum gt_packet_type
{
	GT_MSG_PING        = 0x00,
	GT_MSG_PING_REPLY  = 0x01,
	GT_MSG_BYE         = 0x02,
	GT_MSG_QUERY_ROUTE = 0x30,
	GT_MSG_VENDOR      = 0x31,
	GT_MSG_VENDOR_STD  = 0x32,
	GT_MSG_PUSH        = 0x40,
	GT_MSG_QUERY       = 0x80,
	GT_MSG_QUERY_REPLY = 0x81,
} gt_packet_type_t;

/*****************************************************************************/

#define VMSG_HDR_LEN     (8)

typedef struct gt_vendor_msg
{
	char          vendor_id[4];
	uint16_t      id;
} gt_vendor_msg_t;

/*
 * Pretend these global variables are enumerated constants...don't squint
 * too hard and you won't see they are global variables.
 */
extern const gt_vendor_msg_t *GT_VMSG_MESSAGES_SUPP,
                             *GT_VMSG_TCP_CONNECT_BACK,
                             *GT_VMSG_HOPS_FLOW,
                             *GT_VMSG_PUSH_PROXY_REQ,
                             *GT_VMSG_PUSH_PROXY_ACK;

/*****************************************************************************/

#define get_guid(pkt)    ((pkt)[0])
#define get_command(pkt) ((pkt)[16])
#define get_ttl(pkt)     ((pkt)[17])
#define get_hops(pkt)    ((pkt)[18])

#define get_payload_len(pkt) \
   	(((pkt)[19])       | ((pkt)[20] << 8) | \
     ((pkt)[21] << 16) | ((pkt)[22] << 24))

/*****************************************************************************/

/* handle little-endian ordering on the network */

#ifdef WORDS_BIGENDIAN
/*
 * It's best to watch out for sign extension, so we copy to temporary
 * unsigned variables here. In case the arguments are signed, all is
 * well.
 *
 * NOTE: this depends on a gcc-extension. If someone wants to port to
 * a big-endian, non-gcc compiler, more work here is needed.
 *
 * NOTE2: the uint{16,32}_t types are guaranteed to be _at least_ the
 * number of bits they say they are (i think), so we discard possibly
 * extra bits by using bitwise-and.
 */
#define vtohl(x)                                           \
    ({                                                     \
        uint32_t _tmp = (x);                               \
        (((_tmp & 0xff) << 24)  | ((_tmp & 0xff00) << 8) | \
        ((_tmp & 0xff0000) >> 8) | ((_tmp >> 24) & 0xff)); \
    })

#define vtohs(x)                                           \
    ({                                                     \
        uint16_t _tmp = (x);                               \
        (((_tmp & 0xff) << 8) | ((_tmp >> 8) & 0xff));     \
    })
#else
#define vtohs(x)    (x)
#define vtohl(x)    (x)
#endif /* WORDS_BIGENDIAN */

#define htovs(x)    vtohs(x)
#define htovl(x)    vtohl(x)

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

GtPacket       *gt_packet_new       (uint8_t cmd, uint8_t ttl, gt_guid_t *guid);
GtPacket       *gt_packet_reply     (GtPacket *packet, uint8_t cmd);
GtPacket       *gt_packet_vendor    (const gt_vendor_msg_t *vmsg);
void            gt_packet_free      (GtPacket *packet);
int             gt_packet_error     (GtPacket *packet);
int             gt_packet_seek      (GtPacket *packet, int offset);

GtPacket       *gt_packet_unserialize (unsigned char *data, size_t len);

/*****************************************************************************/

gt_guid_t      *gt_packet_guid         (GtPacket *packet);
uint32_t        gt_packet_payload_len  (GtPacket *packet);
uint32_t        gt_packet_command      (GtPacket *packet);
uint8_t         gt_packet_hops         (GtPacket *packet);
uint8_t         gt_packet_ttl          (GtPacket *packet);

/*****************************************************************************/

void            gt_packet_set_guid         (GtPacket *packet, gt_guid_t *guid);
void            gt_packet_set_payload_len  (GtPacket *packet, uint32_t len);
void            gt_packet_set_command      (GtPacket *packet, uint8_t cmd);
void            gt_packet_set_hops         (GtPacket *packet, uint8_t hops);
void            gt_packet_set_ttl          (GtPacket *packet, uint8_t ttl);

/*****************************************************************************/

uint32_t        gt_packet_get_uint   (GtPacket *packet, size_t size,
                                      int endian, int swap);

uint8_t         gt_packet_get_uint8  (GtPacket *packet);
uint16_t        gt_packet_get_uint16 (GtPacket *packet);
uint32_t        gt_packet_get_uint32 (GtPacket *packet);

in_addr_t       gt_packet_get_ip     (GtPacket *packet);
in_port_t       gt_packet_get_port   (GtPacket *packet);

char           *gt_packet_get_str    (GtPacket *packet);
unsigned char  *gt_packet_get_ustr   (GtPacket *packet, size_t len);

/*****************************************************************************/

int         gt_packet_put_uint8  (GtPacket *packet, uint8_t byte);
int         gt_packet_put_uint16 (GtPacket *packet, uint16_t bytes);
int         gt_packet_put_uint32 (GtPacket *packet, uint32_t bytes);

int         gt_packet_put_str    (GtPacket *packet, const char *str);
int         gt_packet_put_ustr   (GtPacket *packet, const unsigned char *ustr,
                                  size_t len);

int         gt_packet_put_ip     (GtPacket *packet, in_addr_t ip);
int         gt_packet_put_port   (GtPacket *packet, in_port_t port);

/*****************************************************************************/

int         gt_packet_send       (TCPC *c, GtPacket *packet);

int         gt_packet_send_fmt   (TCPC *c, uint8_t cmd,
                                  gt_guid_t *guid, uint8_t ttl,
                                  uint8_t hops, char *fmt, ...)
                                    FORMAT_ATTRIBUTE (printf, 6, 7);

int         gt_packet_reply_fmt  (TCPC *c, GtPacket *packet,
                                  uint8_t cmd, char *fmt, ...)
                                    FORMAT_ATTRIBUTE (printf, 4, 5);


/*****************************************************************************/

void gt_packet_log (GtPacket *packet, struct tcp_conn *src, int sent);

/*****************************************************************************/

#endif /* GIFT_GT_PACKET_H_ */
