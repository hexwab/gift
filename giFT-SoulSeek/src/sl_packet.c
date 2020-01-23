/*
 * Copyright (C) 2003 Arend van Beelen jr. (arend@auton.nl)
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

#include "sl_soulseek.h"
#include "sl_packet.h"
#include "sl_utils.h"

#include <zlib.h>

#define UNCOMPRESS_BUFFER_LENGTH 1048576 // 1M

static uint8_t *uncompress_buffer = NULL;

static void dump_data(unsigned char *data, int len)
{
	while (len--)
		printf("%.2x ", *(data++));

	printf("\n");
}

/*****************************************************************************/

// allocates and returns a packet
SLPacket *sl_packet_create()
{
	SLPacket *packet = (SLPacket *) MALLOC(sizeof(SLPacket));

	packet->length = sizeof(uint32_t);
	packet->type = 0;
	packet->type_byte = FALSE;
	packet->first_data = NULL;
	packet->current_data = NULL;

	return packet;
}

// allocates, constructs and returns a packet. construction is done according to the given scheme
// returns null if data doesn't match the given scheme
SLPacket *sl_packet_create_from_data(unsigned char *data)
{
	SLPacket *packet = (SLPacket *) MALLOC(sizeof(SLPacket));

	assert(data != NULL);
	assert(*((uint32_t *) data) >= sizeof(uint32_t));

	memcpy(&packet->length, data, sizeof(uint32_t));
	memcpy(&packet->type, data + sizeof(uint32_t), sizeof(uint32_t));
	packet->type_byte = FALSE;
	packet->first_data = (SLPacketDataChain *) MALLOC(sizeof(SLPacketDataChain));
	packet->first_data->data.type = SLUnknown;
	packet->first_data->data.data.raw = sl_string_create_empty();
	assert(packet->first_data->data.data.raw != NULL);
	packet->first_data->data.data.raw->length = packet->length - sizeof(uint32_t);
	packet->first_data->data.data.raw->maxlength = packet->length - sizeof(uint32_t);
	packet->first_data->data.data.raw->contents = MALLOC(packet->first_data->data.data.raw->length + 3);
	memcpy(packet->first_data->data.data.raw->contents, data + 2 * sizeof(uint32_t),
	       packet->first_data->data.data.raw->length);
	packet->first_data->next_data = NULL;
	packet->current_data = packet->first_data;

	return packet;
}

// get a byte from the current position from the packet
// argument error (if not NULL) is TRUE if the current data element isn't a byte, FALSE otherwise
uint8_t sl_packet_get_byte(SLPacket *packet, int *error)
{
	SLPacketDataChain *new_data;
	sl_string *raw_data;
	uint8_t byte;

	assert(packet != NULL);

	if(packet->current_data == NULL || (packet->current_data->data.type != SLByte &&
	                                    packet->current_data->data.type != SLUnknown))
	{
		if(error != NULL)
			*error = TRUE;
		return 0;
	}

	if(packet->current_data->data.type == SLUnknown)
	{
		assert(packet->current_data->data.data.raw != NULL);
		if(packet->current_data->data.data.raw->length < sizeof(uint8_t))
		{
			if(error != NULL)
				*error = TRUE;
			return 0;
		}

		if(packet->current_data->data.data.raw->length != sizeof(uint8_t))
		{
			new_data = MALLOC(sizeof(SLPacketDataChain));
			new_data->data.type = SLUnknown;
			new_data->data.data.raw = packet->current_data->data.data.raw;
			new_data->next_data = packet->current_data->next_data;
		}
		else
		{
			new_data = NULL;
		}
		packet->current_data->next_data = (_SLPacketDataChain *) new_data;
		packet->current_data->data.type = SLByte;
		raw_data = packet->current_data->data.data.raw;
		memcpy(&packet->current_data->data.data.byte, raw_data->contents, sizeof(uint8_t));

		if(new_data != NULL)
		{
			new_data->data.data.raw->length -= sizeof(uint8_t);
			memmove(new_data->data.data.raw->contents,
			        new_data->data.data.raw->contents + sizeof(uint8_t),
			        new_data->data.data.raw->length);
		}
		else
		{
			sl_string_destroy(raw_data);
		}
	}

	byte = packet->current_data->data.data.byte;
	packet->current_data = (SLPacketDataChain *) packet->current_data->next_data;
	if(error != NULL)
		*error = FALSE;
	return byte;
}

// get an integer from the current position from the packet
// argument error (if not NULL) is TRUE if the current data element isn't an integer, FALSE otherwise
uint32_t sl_packet_get_integer(SLPacket *packet, int *error)
{
	SLPacketDataChain *new_data;
	sl_string *raw_data;
	uint32_t integer;

	assert(packet != NULL);

	if(packet->current_data == NULL || (packet->current_data->data.type != SLInteger &&
	                                    packet->current_data->data.type != SLUnknown))
	{
		if(error != NULL)
			*error = TRUE;
		return 0;
	}

	if(packet->current_data->data.type == SLUnknown)
	{
		assert(packet->current_data->data.data.raw != NULL);
		if(packet->current_data->data.data.raw->length < sizeof(uint32_t))
		{
			if(error != NULL)
				*error = TRUE;
			return 0;
		}

		if(packet->current_data->data.data.raw->length != sizeof(uint32_t))
		{
			new_data = MALLOC(sizeof(SLPacketDataChain));
			new_data->data.type = SLUnknown;
			new_data->data.data.raw = packet->current_data->data.data.raw;
			new_data->next_data = packet->current_data->next_data;
		}
		else
		{
			new_data = NULL;
		}
		packet->current_data->next_data = (_SLPacketDataChain *) new_data;
		packet->current_data->data.type = SLInteger;
		raw_data = packet->current_data->data.data.raw;
		memcpy(&packet->current_data->data.data.integer, raw_data->contents, sizeof(uint32_t));

		if(new_data != NULL)
		{
			new_data->data.data.raw->length -= sizeof(uint32_t);
			memmove(new_data->data.data.raw->contents,
			        new_data->data.data.raw->contents + sizeof(uint32_t),
			        new_data->data.data.raw->length);
		}
		else
		{
			sl_string_destroy(raw_data);
		}
	}

	integer = packet->current_data->data.data.integer;
	packet->current_data = (SLPacketDataChain *) packet->current_data->next_data;
	if(error != NULL)
		*error = FALSE;
	return integer;
}

// get a string from the current position from the packet
// returns NULL if the current data element isn't a string
sl_string *sl_packet_get_string(SLPacket *packet)
{
	SLPacketDataChain *new_data;
	sl_string *raw_data,
	            *string;

	assert(packet != NULL);

	if(packet->current_data == NULL || (packet->current_data->data.type != SLString &&
	                                    packet->current_data->data.type != SLUnknown))
	{
		return NULL;
	}

	if(packet->current_data->data.type == SLUnknown)
	{
		assert(packet->current_data->data.data.raw != NULL);
		assert(packet->current_data->data.data.raw->contents != NULL);
		if(packet->current_data->data.data.raw->length < sizeof(uint32_t) ||
		   packet->current_data->data.data.raw->length < *((uint32_t *) (packet->current_data->data.data.raw->contents)) + sizeof(uint32_t))
		{
			return NULL;
		}

		if(packet->current_data->data.data.raw->length != *((uint32_t *) (packet->current_data->data.data.raw->contents)) + sizeof(uint32_t))
		{
			new_data = MALLOC(sizeof(SLPacketDataChain));
			new_data->data.type = SLUnknown;
			new_data->data.data.raw = packet->current_data->data.data.raw;
			new_data->next_data = packet->current_data->next_data;
		}
		else
		{
			new_data = NULL;
		}
		packet->current_data->next_data = (_SLPacketDataChain *) new_data;
		packet->current_data->data.type = SLString;
		raw_data = packet->current_data->data.data.raw;
		packet->current_data->data.data.string = sl_string_create_from_data(raw_data->contents);
		assert(packet->current_data->data.data.string != NULL);

		if(new_data != NULL)
		{
			new_data->data.data.raw->length -= packet->current_data->data.data.string->length + sizeof(uint32_t);
			memmove(new_data->data.data.raw->contents,
			        new_data->data.data.raw->contents + packet->current_data->data.data.string->length + sizeof(uint32_t),
			        new_data->data.data.raw->length);
		}
		else
		{
			sl_string_destroy(raw_data);
		}
	}

	string = packet->current_data->data.data.string;
	packet->current_data = (SLPacketDataChain *) packet->current_data->next_data;
	return string;
}

// decompresses a ZLib compressed data block
int sl_packet_decompress_block(SLPacket *packet)
{
	uLongf length = UNCOMPRESS_BUFFER_LENGTH;

	if(packet->current_data == NULL ||
	   packet->current_data->data.type != SLUnknown)
		return FALSE;
	
	if(uncompress_buffer == NULL)
	{
		uncompress_buffer = MALLOC(UNCOMPRESS_BUFFER_LENGTH);
	}

	if(uncompress(uncompress_buffer, &length, packet->current_data->data.data.raw->contents,
	              packet->current_data->data.data.raw->length) != Z_OK)
		return FALSE;

	free(packet->current_data->data.data.raw->contents);
	packet->current_data->data.data.raw->contents = MALLOC(length);
	memcpy(packet->current_data->data.data.raw->contents, uncompress_buffer, length);
	packet->current_data->data.data.raw->length = length;
	packet->current_data->data.data.raw->maxlength = length;

	return TRUE;
}

// insert a byte into the packet
int sl_packet_insert_byte(SLPacket *packet, uint8_t byte)
{
	SLPacketDataChain *new_data = (SLPacketDataChain *) MALLOC(sizeof(SLPacketDataChain));

	assert(packet != NULL);

	new_data->data.type = SLByte;
	new_data->data.data.byte = byte;

	if(packet->current_data == NULL)
	{
		packet->current_data = new_data;
		new_data->next_data = NULL;
		packet->first_data = new_data;
	}
	else
	{
		new_data->next_data = packet->current_data->next_data;
		packet->current_data->next_data = (_SLPacketDataChain *) new_data;
		packet->current_data = new_data;
	}
	packet->length += 1;

	return TRUE;
}

// insert an integer into the packet
int sl_packet_insert_integer(SLPacket *packet, uint32_t integer)
{
	SLPacketDataChain *new_data = (SLPacketDataChain *) MALLOC(sizeof(SLPacketDataChain));

	assert(packet != NULL);

	new_data->data.type = SLInteger;
	new_data->data.data.integer = integer;

	if(packet->current_data == NULL)
	{
		packet->current_data = new_data;
		packet->current_data->next_data = NULL;
		packet->first_data = new_data;
	}
	else
	{
		new_data->next_data = packet->current_data->next_data;
		packet->current_data->next_data = (_SLPacketDataChain *) new_data;
		packet->current_data = new_data;
	}
	packet->length += 4;

	return TRUE;
}

// insert a string into the packet
int sl_packet_insert_string(SLPacket *packet, sl_string *string)
{
	SLPacketDataChain *new_data = (SLPacketDataChain *) MALLOC(sizeof(SLPacketDataChain));

	assert(packet != NULL);

	new_data->data.type = SLString;
	new_data->data.data.string = sl_string_copy(string);

	if(packet->current_data == NULL)
	{
		packet->current_data = new_data;
		packet->current_data->next_data = NULL;
		packet->first_data = new_data;
	}
	else
	{
		new_data->next_data = packet->current_data->next_data;
		packet->current_data->next_data = (_SLPacketDataChain *) new_data;
		packet->current_data = new_data;
	}
	packet->length += string->length + 4;

	return TRUE;
}

// insert a C string into the packet
int sl_packet_insert_c_string(SLPacket *packet, char *string)
{
	sl_string *str = sl_string_create_with_contents(string);
	sl_packet_insert_string(packet, str);
	sl_string_destroy(str);

	return TRUE;
}

// sets the length of the packet
void sl_packet_set_length(SLPacket *packet, uint32_t length)
{
	assert(packet != NULL);

	packet->length = length;
}

// gets the length of the packet
uint32_t sl_packet_get_length(SLPacket *packet)
{
	assert(packet != NULL);

	return packet->length;
}

// sets the type of the packet
void sl_packet_set_type(SLPacket *packet, uint32_t type)
{
	assert(packet != NULL);

	packet->type = type;
}

// call this if the type is a single byte (it's an ugly hack in the protocol, I know)
void sl_packet_set_type_byte(SLPacket *packet)
{
	assert(packet != NULL);

	if((packet->current_data != NULL && packet->current_data->data.type != SLUnknown) ||
	   packet->type_byte == TRUE)
		return;

	if(packet->current_data != NULL)
	{
		memmove(packet->current_data->data.data.raw->contents + 3,
		        packet->current_data->data.data.raw->contents,
		        packet->current_data->data.data.raw->length);
		memcpy(packet->current_data->data.data.raw->contents,
		       (unsigned char *) &packet->type + 1, 3);
		packet->current_data->data.data.raw->length += 3;
		packet->current_data->data.data.raw->maxlength += 3;
	}
	else
	{
		packet->length = sizeof(uint8_t);
	}

	packet->type &= 0x000000ff;
	packet->type_byte = TRUE;
}

// gets the type of the packet
uint32_t sl_packet_get_type(SLPacket *packet)
{
	assert(packet != NULL);

	return packet->type;
}

// sets the current data position back to the first element
void sl_packet_rewind(SLPacket *packet)
{
	assert(packet != 0);

	packet->current_data = packet->first_data;
}

// destroys a previously allocated packet
void sl_packet_destroy(SLPacket *packet)
{
	SLPacketDataChain *current_datatype = packet->first_data,
	                    *temp = NULL;

	assert(packet != NULL);

	while(current_datatype != NULL)
	{
		temp = (SLPacketDataChain *) current_datatype->next_data;
		if(current_datatype->data.type == SLString ||
		   current_datatype->data.type == SLUnknown)
		{
			// this will work for SLUnknown blocks as string and raw are in fact a union
			sl_string_destroy(current_datatype->data.data.string);
		}
		free(current_datatype);
		current_datatype = temp;
	}

	free(packet);
}

/*****************************************************************************/

// sends a packet over a socket
void sl_packet_send(TCPC *conn, SLPacket *packet)
{
	uint8_t  *buffer;
	uint32_t  pos;
	SLPacketDataChain *part;

	assert(conn != NULL);
	assert(packet != NULL);

	buffer = (uint8_t *) MALLOC(packet->length + sizeof(uint32_t));
	memcpy(buffer, &packet->length, sizeof(uint32_t));
	pos = sizeof(uint32_t);
	if(packet->type_byte == FALSE)
	{
		memcpy(buffer + sizeof(uint32_t), &packet->type, sizeof(uint32_t));
		pos += sizeof(uint32_t);
	}
	else
	{
		memcpy(buffer + sizeof(uint32_t), &packet->type, sizeof(uint8_t));
		pos += sizeof(uint8_t);
	}
	part = packet->first_data;
	while(part != NULL)
	{
		switch(part->data.type)
		{
			case SLUnknown:
				memcpy(buffer + pos, part->data.data.raw->contents, part->data.data.raw->length);
				pos += part->data.data.string->length;
				break;

			case SLByte:
				memcpy(buffer + pos, &part->data.data.byte, sizeof(uint8_t));
				pos += sizeof(uint8_t);
				break;

			case SLInteger:
				memcpy(buffer + pos, &part->data.data.integer, sizeof(uint32_t));
				pos += sizeof(uint32_t);
				break;

			case SLString:
				memcpy(buffer + pos, &part->data.data.string->length, sizeof(uint32_t));
				pos += sizeof(uint32_t);
				memcpy(buffer + pos, part->data.data.string->contents, part->data.data.string->length);
				pos += part->data.data.string->length;
				break;
		}
		part = (SLPacketDataChain *) part->next_data;
	}
	tcp_write(conn, buffer, packet->length + sizeof(uint32_t));
	free(buffer);
}

/*****************************************************************************/
