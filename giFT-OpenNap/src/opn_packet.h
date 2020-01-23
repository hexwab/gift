/* giFT OpenNap
 * Copyright (C) 2003 Tilman Sauerbeck <tilman@code-monkey.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __OPN_PACKET_H
#define __OPN_PACKET_H

/* Each message to/from the server is in the form of
 * <length><type><data>
 * where <length> and <type> are 2 bytes each.
 * <length> specifies the length in bytes of the <data> portion of the message.
 *
 * Be aware that <length> and <type> are in little-endian format.
 * 
 * The <data> portion of the message is a plain ASCII string.
 */

/* length of the header: sizeof(length) + sizeof(type) */
#define OPN_PACKET_HEADER_LEN 4 

typedef struct _OpnPacket {
	OpnCommand cmd;
	
	char *data; /* payload */
	uint16_t data_size; /* payload's size */
	
	unsigned char *serialized; /* serialized data */
} OpnPacket;

OpnPacket *opn_packet_new(OpnCommand cmd, char *data, uint16_t size);
void opn_packet_free(OpnPacket *packet);

OpnPacket *opn_packet_unserialize(unsigned char *data, uint16_t size);

BOOL opn_packet_send(OpnPacket *packet, TCPC *con);
BOOL opn_packet_recv(TCPC *con, void *udata);
	
#endif

