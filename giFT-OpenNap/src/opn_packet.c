/* giFT OpenNap
 * Copyright (C) 2003 Tilman Sauerbeck <tilman@code-monkey.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "opn_opennap.h"

/* Sets the payload of an OpnPacket
 * @param packet The OpnPacket whose payload is set
 * @data The payload of the OpnPacket
 * @size The size of the payload
 */
static BOOL packet_set_data(OpnPacket *packet, char *data, uint16_t size)
{
	if (size > UINT16_MAX || !data || !size)
		return FALSE;

	free(packet->data);

	if (!(packet->data = malloc(size)))
		return FALSE;

	memcpy(packet->data, data, size);
	packet->data_size = size;

	return TRUE;
}

/* Creates a new OpnPacket
 * @param cmd The command for the new OpnPacket
 * @param data The payload of the packet
 * @param size The size of the payload
 */
OpnPacket *opn_packet_new(OpnCommand cmd, char *data, uint16_t size)
{
	OpnPacket *packet;

	assert(cmd != OPN_CMD_NONE);

	if (!(packet = malloc(sizeof(OpnPacket))))
		return NULL;

	memset(packet, 0, sizeof(OpnPacket));

	packet->cmd = cmd;

	if (size && data)
		if (!packet_set_data(packet, data, size)) {
			free(packet);
			packet = NULL;
		}

	return packet;
}

/* Frees a OpnPacket
 * @param packet The OpnPacket which should be freed
 */
void opn_packet_free(OpnPacket *packet)
{
	if (!packet)
		return;

	free(packet->data);
	free(packet->serialized);
	free(packet);
}

/* Serializes a OpnPacket
 * @param packet The OpnPacket which should be serialized
 * @return A pointer to the serialized data. This should not be freed!
 */
static unsigned char *packet_serialize(OpnPacket *packet, long *ssize)
{
	uint16_t foo;
	
	if (!packet)
		return NULL;

	free(packet->serialized);
	
	/* payload of the message + type and size fields */
	if (!(packet->serialized = malloc(packet->data_size + OPN_PACKET_HEADER_LEN)))
		return NULL;

	/* size and type are always in little-endian format */
	foo = packet->data_size;
	//me2le_16(foo);
	memcpy(packet->serialized, &foo, 2);

	foo = packet->cmd;
	//me2le_16(foo);
	memcpy(&packet->serialized[2], &foo, 2);

	if (packet->data_size)
		memcpy(&packet->serialized[OPN_PACKET_HEADER_LEN], packet->data, packet->data_size);
	
	if (ssize)
		*ssize = OPN_PACKET_HEADER_LEN + packet->data_size;
	
	return packet->serialized;
}

/* Unserializes a buffer into a OpnPacket.
 * @param data Data stream which is to be unserialized
 * @param size Amount of bytes to unserialize
 * @return The newly created OpnPacket
 */
OpnPacket *opn_packet_unserialize(unsigned char *data, uint16_t size)
{
	OpnPacket *packet;

	assert(size >= OPN_PACKET_HEADER_LEN);

	if (!(packet = malloc(sizeof(OpnPacket))))
		return NULL;

	memset(packet, 0, sizeof(OpnPacket));

	/* FIXME endianess! */
	memcpy(&packet->data_size, data, 2);
	memcpy(&packet->cmd, &data[2], 2);
	
	if ((size -= OPN_PACKET_HEADER_LEN)) {
		if (!(packet->data = malloc(size + 1))) {
			free(packet);
			return NULL;
		}
		
		memcpy(packet->data, &data[OPN_PACKET_HEADER_LEN], size);
		packet->data[size] = 0;
	}

	return packet;
}

/* Sends a packet
 * @param packet The packet to send
 * @param con The connection the packet is sent over
 * @return TRUE on success, FALSE on failure
 */
BOOL opn_packet_send(OpnPacket *packet, TCPC *con)
{
	unsigned char *data;
	long len = 0;

	if (!(data = packet_serialize(packet, &len)))
		return FALSE;
	
	tcp_send(con, data, len);

	return TRUE;
}

/* Reads a packet and handles it.
 * @param con The connection to read the packet from
 * @param udata Arbitrary data probably needed in the handler functions
 * @return TRUE if a packet has been read, else FALSE.
 */
BOOL opn_packet_recv(TCPC *con, void *udata)
{
	OpnPacket *packet;
	unsigned char buf[2048];
	int bytes;
	uint16_t len;
	
	/* get the length field of the message
	 * FIXME: endianess!
	 */
	tcp_peek(con, (unsigned char *) &len, 2);
	
	len = MIN(len + OPN_PACKET_HEADER_LEN, sizeof(buf));

	if ((bytes = tcp_recv(con, buf, len)) < OPN_PACKET_HEADER_LEN)
		return FALSE;
	
	packet = opn_packet_unserialize(buf, bytes);
	
	opn_protocol_handle(packet, udata);
	opn_packet_free(packet);

	return TRUE;
}

