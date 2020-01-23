/*
 * ft_packet.h
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

#ifndef __FT_PACKET_H
#define __FT_PACKET_H

/*****************************************************************************/

/**
 * OR'd with command to indicate that the data stream in the packet actually
 * belongs to a stream.
 */
#define FT_PACKET_STREAM  (1 << 15)

#define FT_PACKET_INITIAL 128          /**< initial alloc size */
#define FT_PACKET_MAX     2048         /**< maximum data size in a packet */
#define FT_PACKET_HEADER  4            /**< header size for all packets */

/**
 * FTPacket structure.
 */
typedef struct
{
	unsigned int  offset;              /**< current read offset */
	ft_uint16     len;                 /**< packet length (excluding header) */
	ft_uint16     command;             /**< command (includes protocol
	                                    *   options) */

	/* serialized data stream, incomplete until ft_packet_serialize is
	 * executed */
	unsigned char *data;               /**< serialized data stream.
	                                    *   incomplete until an
	                                    *   ::ft_packet_serialize call. */
	size_t         data_len;           /**< allocate size to data */
} FTPacket;

/*****************************************************************************/

/**
 * Allocate a new FTPacket structure.
 *
 * @param cmd   Command this packet issues.
 * @param flags Packet flags to be OR'd at send.
 */
FTPacket *ft_packet_new (ft_uint16 cmd, ft_uint16 flags);

/**
 * Duplicate an FTPacket structure.  This includes duplication of the data
 * stream.
 */
FTPacket *ft_packet_dup (FTPacket *packet);

/**
 * Unallocate an FTPacket structure.
 */
void ft_packet_free (FTPacket *packet);

/*****************************************************************************/

/**
 * Serialize an FTPacket.  Format the data stream appropriately for writing.
 *
 * @param packet
 * @param s_len  Storage location for the serialized length.
 *
 * @return Serialized data stream.
 */
unsigned char *ft_packet_serialize (FTPacket *packet, size_t *s_len);

/**
 * Unserialize a data stream.  Instantiate the serialized data stream into an
 * FTPacket.
 *
 * @param data Serialized data stream.
 * @param len  Length of \em data.
 *
 * @return Unserialized FTPacket.
 */
FTPacket *ft_packet_unserialize (unsigned char *data, size_t len);

/*****************************************************************************/

/**
 * Set packet command.
 */
void ft_packet_set_command (FTPacket *packet, ft_uint16 cmd);

/**
 * Set packet flags.
 */
void ft_packet_set_flags (FTPacket *packet, ft_uint16 flags);

/**
 * Set packet length.
 */
void ft_packet_set_length (FTPacket *packet, ft_uint16 len);

/**
 * Get packet command.
 */
ft_uint16 ft_packet_command (FTPacket *packet);

/**
 * Get packet flags.
 */
ft_uint16 ft_packet_flags (FTPacket *packet);

/**
 * Get packet length.
 */
ft_uint16 ft_packet_length (FTPacket *packet);

/*****************************************************************************/

/**
 * Append an unsigned integer.
 *
 * @param packet
 * @param data    Storage location of integer.
 * @param size    Size of integer.
 * @param ho      Value provided is in host order and should be converted.
 *
 * @returns Boolean success or failure.
 */
int ft_packet_put_uint (FTPacket *packet, void *data, size_t size, int ho);

/**
 * @see ft_packet_put_uint
 */
int ft_packet_put_uint8 (FTPacket *packet, ft_uint8 data);

/**
 * @see ft_packet_put_uint
 */
int ft_packet_put_uint16 (FTPacket *packet, ft_uint16 data, int host_order);

/**
 * @see ft_packet_put_uint
 */
int ft_packet_put_uint32 (FTPacket *packet, ft_uint32 data, int host_order);

/**
 * Append an IPv4 address.
 *
 * @see ft_packeet_put_uint
 */
int ft_packet_put_ip (FTPacket *packet, in_addr_t ip);

/**
 * Append an unsigned character array.
 *
 * @param packet
 * @param str    Storage location of the array to append.
 * @param len    Length of \em str.  -1 will call strlen for you.
 *
 * @see ft_packet_put_uint
 */
int ft_packet_put_ustr (FTPacket *packet, unsigned char *str, int len);

/**
 * Append an ASCII string.
 *
 * @see ft_packet_put_ustr
 */
int ft_packet_put_str (FTPacket *packet, char *str);

/*****************************************************************************/

ft_uint32      ft_packet_get_uint   (FTPacket *packet, size_t size,
                                     int host_order);
ft_uint8       ft_packet_get_uint8  (FTPacket *packet);
ft_uint16      ft_packet_get_uint16 (FTPacket *packet, int host_order);
ft_uint32      ft_packet_get_uint32 (FTPacket *packet, int host_order);
in_addr_t      ft_packet_get_ip     (FTPacket *packet);
unsigned char *ft_packet_get_ustr   (FTPacket *packet, size_t len);
char          *ft_packet_get_str    (FTPacket *packet);
void          *ft_packet_get_array  (FTPacket *packet, size_t size,
                                     size_t nmemb, int host_order);

/*****************************************************************************/

/**
 * Send an FTPacket over an established socket.
 */
int ft_packet_send (Connection *c, FTPacket *packet);

/**
 * Ensure that the packet is appropriately delivered to a potentially
 * unestablished socket connection.
 */
int ft_packet_sendto (in_addr_t to, FTPacket *packet);

/**
 * Wrapper around ::ft_packet_send for variable argument packet
 * construction.
 */
int ft_packet_sendva (Connection *c, ft_uint16 cmd, ft_uint16 flags,
                      char *fmt, ...);

/*****************************************************************************/

/**
 * Controls the human readable formatting of an FTPacket structure.
 *
 * @param packet
 *
 * @return Flat representation of \em packet.
 */
char *ft_packet_fmt (FTPacket *packet);

/*****************************************************************************/

#endif /* __FT_PACKET_H */