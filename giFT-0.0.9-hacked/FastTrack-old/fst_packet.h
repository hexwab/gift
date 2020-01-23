/*
 * Copyright (C) 2003 Markus Kern (mkern@users.sourceforge.net)
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

#ifndef __FST_PACKET_H
#define __FST_PACKET_H

#include "fst_fasttrack.h"
#include "fst_crypt.h"

/*****************************************************************************/

/**
 * FSTPacket structure.
 */
typedef struct
{
	unsigned char *data;		// pointer to allocated memory
	unsigned char *read_ptr;	// pointer to current read position
	int used;					// used number of bytes relative to p_mem
	int allocated;				// allocated number of bytes
}FSTPacket;

/*****************************************************************************/

// allocates and returns a new packet
FSTPacket *fst_packet_create();

// creates packet with data from packet->read_ptr to packet->read_ptr + len and moves read_ptr
FSTPacket *fst_packet_create_copy(FSTPacket* packet, size_t len);

// appends everything from append->read_ptr to packet
void fst_packet_append(FSTPacket *packet, FSTPacket *append);

// frees packet
void fst_packet_free(FSTPacket *packet);

// rewinds read_ptr to data
void fst_packet_rewind(FSTPacket *packet);

// removes everything from before read_ptr from packet
void fst_packet_truncate(FSTPacket *packet);

// returns size of entire packet
int fst_packet_size(FSTPacket* packet);

// returns size of remaining data
int fst_packet_remaining(FSTPacket* packet);

/*****************************************************************************/

// append uint8 to packet
void fst_packet_put_uint8 (FSTPacket *packet, fst_uint8 data);

// append uint16 to packet
void  fst_packet_put_uint16 (FSTPacket *packet, fst_uint16 data);

// append uint32 to packet
void  fst_packet_put_uint32 (FSTPacket *packet, fst_uint32 data);

// append string of length len to packet
void  fst_packet_put_ustr (FSTPacket *packet, unsigned char *str, size_t len);

// append FastTrack dynamic int to packet
void  fst_packet_put_dynint (FSTPacket *packet, fst_uint32 data);

/*****************************************************************************/

// returns uint8 and moves read_ptr
fst_uint8 fst_packet_get_uint8 (FSTPacket *packet);

// returns uint16 and moves read_ptr
fst_uint16 fst_packet_get_uint16 (FSTPacket *packet);

// returns uint32 and moves read_ptr
fst_uint32 fst_packet_get_uint32 (FSTPacket *packet);

// returns string and moves read_ptr, caller frees returned string
unsigned char *fst_packet_get_ustr (FSTPacket *packet, size_t len);

// returns zero terminated string and moves read_ptr, caller frees returned string
char *fst_packet_get_str (FSTPacket *packet, size_t len);

// reads FastTrack dynamic int and moves read_ptr
fst_uint32 fst_packet_get_dynint (FSTPacket *packet);

// counts the number of bytes from read_ptr until termbyte is reached
// returns -1 if termbyte doesn't occur in packet
int fst_packet_strlen (FSTPacket *packet, fst_uint8 termbyte);

/*****************************************************************************/

// encrypt entire packet using cipher
void fst_packet_encrypt(FSTPacket *packet, FSTCipher *cipher);

// decrypt entire packet using cipher
void fst_packet_decrypt(FSTPacket *packet, FSTCipher *cipher);

/*****************************************************************************/

// send entire packet to connected host
int fst_packet_send (FSTPacket *packet, TCPC *tcpcon);

// recv packet from connected host and append data
int fst_packet_recv (FSTPacket *packet, TCPC *tcpcon);

/*****************************************************************************/

#endif /* __FST_PACKET_H */
