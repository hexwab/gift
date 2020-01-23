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

#ifndef __SL_PACKET_H
#define __SL_PACKET_H

#include "sl_soulseek.h"
#include "sl_string.h"

/*****************************************************************************/

// list of packet types
typedef enum
{
	// server/client packets
	SLPierceFW =               0,
	SLLogin =                  1,
	SLSetWaitPort =            2,
	SLGetPeerAddress =         3,
	SLAddUser =                5,
	SLGetUserStatus =          7,
	SLSayChatroom =           13,
	SLJoinRoom =              14,
	SLLeaveRoom =             15,
	SLUserJoinedRoom =        16,
	SLUserLeftRoom =          17,
	SLConnectToPeer =         18,
	SLMessageUser =           22,
	SLMessageAcked =          23,
	SLFileSearch =            26,
	SLSetStatus =             28,
	SLSharedFoldersFiles =    35,
	SLGetUserStats =          36,
	SLQueuedDownloads =       40,
	SLPlaceInLineResponse =   60,
	SLRoomAdded =             62,
	SLRoomRemoved =           63,
	SLRoomList =              64,
	SLExactFileSearch =       65,
	SLAdminMessage =          66,
	SLGlobalUserList =        67,
	SLTunneledMessage =       68,
	SLPrivilegedUsers =       69,
	SLAddToPrivileged =       91,
	SLCheckPrivileges =       92,
	SLCantConnectToPeer =   1001,

	// peer/peer packets
	SLPeerInit =               1,
	SLGetSharedFileList =      4,
	SLSharedFileList =         5,
	SLFileSearchResult =       9,
	SLUserInfoRequest =       15,
	SLUserInfoReply =         16,
	SLFolderContentsRequest = 36,
	SLFolderContentsResponse =37,
	SLTransferRequest =       40,
	SLTransferResponse =      41,
	SLPlaceholdUpload =       42,
	SLQueueUpload =           43,
	SLPlaceInQueue =          44,
	SLUploadFailed =          46,
	SLQueueFailed =           50,
	SLPlaceInQueueRequest =   51
} SLPacketTypes;

// different types of data that exist in packets
typedef enum
{
	SLUnknown,
	SLByte,
	SLInteger,
	SLString
} SLDataTypes;

/**
 * SLDataType structure
 */
typedef struct
{
	SLDataTypes type; // the type of datatype
	union
	{
		sl_string *raw;
		uint8_t      byte;
		uint32_t     integer;
		sl_string *string;
	} data;
} SLDataType;

/**
 * SLPacketDataChain structure
 */
typedef struct SLPacketDataChain _SLPacketDataChain;
typedef struct
{
	SLDataType          data;      // the data in this element
	_SLPacketDataChain *next_data; // pointer to the next data element
} SLPacketDataChain;

/**
 * SLPacket structure
 */
typedef struct
{
	uint32_t length;                // total length of packet minus the 4 bytes for length
	uint32_t type;                  // type of packet
	int type_byte;                     // is the type a single byte?
	SLPacketDataChain *first_data;   // pointer first data element
	SLPacketDataChain *current_data; // current data element
} SLPacket;

/*****************************************************************************/

// allocates and returns a packet
SLPacket *sl_packet_create();

// allocates, constructs and returns a packet. construction is done according to the given scheme
// returns null if data doesn't match the given scheme
SLPacket *sl_packet_create_from_data(unsigned char *data);

// get a byte from the current position from the packet
// argument error (if not NULL) is TRUE if the current data element isn't a byte, FALSE otherwise
uint8_t sl_packet_get_byte(SLPacket *packet, int *error);

// get an integer from the current position from the packet
// argument error (if not NULL) is TRUE if the current data element isn't an integer, FALSE otherwise
uint32_t sl_packet_get_integer(SLPacket *packet, int *error);

// get a string from the current position from the packet
// returns NULL if the current data element isn't a string
sl_string *sl_packet_get_string(SLPacket *packet);

// decompresses a ZLib compressed data block
int sl_packet_decompress_block(SLPacket *packet);

// insert a byte into the packet
int sl_packet_insert_byte(SLPacket *packet, uint8_t byte);

// insert an integer into the packet
int sl_packet_insert_integer(SLPacket *packet, uint32_t integer);

// insert a string into the packet
int sl_packet_insert_string(SLPacket *packet, sl_string *string);

// insert a C string into the packet
int sl_packet_insert_c_string(SLPacket *packet, char *string);

// sets the length of the packet
void sl_packet_set_length(SLPacket *packet, uint32_t length);

// gets the length of the packet
uint32_t sl_packet_get_length(SLPacket *packet);

// sets the type of the packet
void sl_packet_set_type(SLPacket *packet, uint32_t type);

// call this if the type is a single byte (it's an ugly hack in the protocol, I know)
void sl_packet_set_type_byte(SLPacket *packet);

// gets the type of the packet
uint32_t sl_packet_get_type(SLPacket *packet);

// sets the current data position back to the first element
void sl_packet_rewind(SLPacket *packet);

// destroys a previously allocated packet
void sl_packet_destroy(SLPacket *packet);

// ***************************************************************************/

// sends a packet over a socket
void sl_packet_send(TCPC *conn, SLPacket *packet);

/*****************************************************************************/

#endif /* __SL_PACKET_H */
