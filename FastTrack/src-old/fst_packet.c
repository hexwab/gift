/*
 * $Id: fst_packet.c,v 1.3 2003/06/26 18:34:37 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#include "fst_fasttrack.h"
#include "fst_packet.h"

/*****************************************************************************/

static int packet_resize (FSTPacket *packet, size_t len);
static int packet_write(FSTPacket *packet, void* data, size_t size);
static int packet_read(FSTPacket *packet, void* data, size_t size);

/*****************************************************************************/

// allocates and returns a new packet
FSTPacket *fst_packet_create()
{
	FSTPacket *packet;

	packet = malloc(sizeof(FSTPacket));
	packet->data = packet->read_ptr = NULL;
	packet->used = packet->allocated = 0;

	return packet;
}

// creates packet with data from packet->read_ptr to packet->read_ptr + len and moves read_ptr
FSTPacket *fst_packet_create_copy(FSTPacket* packet, size_t len)
{
	FSTPacket *new_packet;

	new_packet = fst_packet_create();

	if(len > fst_packet_remaining(packet))
		len = fst_packet_remaining(packet);

	packet_write(new_packet, packet->read_ptr, len);
	packet->read_ptr += len;
	
	return new_packet;
}

// appends everything from append->read_ptr to packet
void fst_packet_append(FSTPacket *packet, FSTPacket *append)
{
	packet_write(packet, append->read_ptr, fst_packet_remaining(append));
}

// frees packet
void fst_packet_free(FSTPacket *packet)
{
	if(!packet)
		return;

	if(packet->data)
		free(packet->data);
	free(packet);
}

// rewinds read_ptr to data
void fst_packet_rewind(FSTPacket *packet)
{
	packet->read_ptr = packet->data;
}

// removes everything from before read_ptr from packet
void fst_packet_truncate(FSTPacket *packet)
{
	size_t remaining = fst_packet_remaining(packet);
	int i;
	unsigned char *src = packet->read_ptr;
	unsigned char *dst = packet->data;

	for(i=0;i<remaining;++i,++src,++dst)
		*dst = *src;

	packet->read_ptr = packet->data;
	packet->used = remaining;
}

// returns size of entire packet
int fst_packet_size(FSTPacket* packet)
{
	return packet->used;
}

// returns size of remaining data
int fst_packet_remaining(FSTPacket* packet)
{
	return packet->used - (packet->read_ptr - packet->data);
}

/*****************************************************************************/

// append uint8 to packet
void fst_packet_put_uint8 (FSTPacket *packet, fst_uint8 data)
{
	packet_write(packet, &data, sizeof(fst_uint8));
}

// append uint16 to packet
void  fst_packet_put_uint16 (FSTPacket *packet, fst_uint16 data)
{
	packet_write(packet, &data, sizeof(fst_uint16));
}

// append uint32 to packet
void  fst_packet_put_uint32 (FSTPacket *packet, fst_uint32 data)
{
	packet_write(packet, &data, sizeof(fst_uint32));
}

// append string of length len to packet
void  fst_packet_put_ustr (FSTPacket *packet, unsigned char *str, size_t len)
{
	packet_write(packet, str, len);
}

// append FastTrack dynamic int to packet
void  fst_packet_put_dynint (FSTPacket *packet, fst_uint32 data)
{
	unsigned char buf[6];
	int len, i;

	// need to fill backwards, so we need to know size in advance
	if(data > 128*128*128*128)
		len = 5;
	else if (data > 128*128*128)
		len = 4;
	else if (data > 128*128)
		len = 3;
	else if (data > 128)
		len = 2;
	else
		len = 1;

	i = len - 1;

	// last byte doesn't have high bit set
	buf[i] = data & 0x7f;
	data >>= 7;
	i--;

	// all others have
	for(; i>=0; i--)
	{
		buf[i] = 0x80 | (fst_uint8)(data & 0x7f);
		data >>= 7;
	}

	packet_write(packet, buf, len);
}

/*****************************************************************************/

// returns uint8 and moves read_ptr
fst_uint8 fst_packet_get_uint8 (FSTPacket *packet)
{
	fst_uint8 ret;
	packet_read(packet, &ret, sizeof(fst_uint8));
	return ret;
}

// returns uint16 and moves read_ptr
fst_uint16 fst_packet_get_uint16 (FSTPacket *packet)
{
	fst_uint16 ret;
	packet_read(packet, &ret, sizeof(fst_uint16));
	return ret;
}

// returns uint32 and moves read_ptr
fst_uint32 fst_packet_get_uint32 (FSTPacket *packet)
{
	fst_uint32 ret;
	packet_read(packet, &ret, sizeof(fst_uint32));
	return ret;
}

// returns string and moves read_ptr, caller frees returned string
unsigned char *fst_packet_get_ustr (FSTPacket *packet, size_t len)
{
	unsigned char *ret;
	ret = malloc(len);
	packet_read(packet, ret, len);
	return ret;
}

// returns zero terminated string and moves read_ptr, caller frees returned string
char *fst_packet_get_str (FSTPacket *packet, size_t len)
{
	char *ret;
	ret = malloc(len+1);
	packet_read(packet, ret, len);
	ret[len] = 0;
	return ret;
}

// reads FastTrack dynamic int and moves read_ptr
fst_uint32 fst_packet_get_dynint (FSTPacket *packet)
{
	fst_uint32 ret = 0;
	fst_uint8 curr;

    do
	{
		if(packet_read(packet, &curr, 1) == FALSE)
			return 0;
		ret <<= 7;
		ret |= (curr & 0x7f);
    } while(curr & 0x80);

    return ret;
}

// counts the number of bytes from read_ptr until termbyte is reached
// returns -1 if termbyte doesn't occur in packet
int fst_packet_strlen (FSTPacket *packet, fst_uint8 termbyte)
{
	unsigned char *p = packet->read_ptr;
	int remaining = fst_packet_remaining (packet);
	int i = 0;

	for(i=0; i < remaining; i++, p++)
		if(*p == termbyte)
			return i;

	return -1;
}

/*****************************************************************************/

// encrypt entire packet using cipher
void fst_packet_encrypt(FSTPacket *packet, FSTCipher *cipher)
{
	fst_cipher_crypt(cipher, packet->data, packet->used);
}

// decrypt entire packet using cipher
void fst_packet_decrypt(FSTPacket *packet, FSTCipher *cipher)
{
	fst_cipher_crypt(cipher, packet->data, packet->used);
}

/*****************************************************************************/

// send entire packet to connected host
int fst_packet_send (FSTPacket *packet, TCPC *tcpcon)
{
	tcp_send(tcpcon, packet->data, packet->used);
	return TRUE;
}

// recv packet from connected host and append data
int fst_packet_recv (FSTPacket *packet, TCPC *tcpcon)
{
	int ret;

	packet_resize(packet, packet->used + 1024);

	ret = tcp_recv(tcpcon, packet->data + packet->used, packet->allocated - packet->used);

	if(ret <= 0)
		return FALSE;

	packet->used += ret;

	return TRUE;
}

/*****************************************************************************/

/* make sure packet->mem is large enough to hold len bytes */
static int packet_resize (FSTPacket *packet, size_t len)
{
	unsigned char *new_mem;
	size_t new_alloc, read_offset;

	if (!packet)
		return FALSE;

	/* realloc (..., 0) == free */
	if (len == 0)
	{ 
		free (packet->data);
		packet->data = packet->read_ptr = NULL;
		packet->used = packet->allocated = 0;
		return TRUE;
	}

	/* the buffer we have allocated is already large enough */
	if (packet->allocated >= len)
		return TRUE;

	/* determine an appropriate resize length */
	new_alloc = packet->allocated;
	while (new_alloc < len)
		new_alloc += 512;

	read_offset = packet->read_ptr - packet->data;

	/* gracefully fail if we are unable to resize the buffer */
	if (!(new_mem = realloc (packet->data, new_alloc)))
		return FALSE;

	/* modify the packet structure to reflect this resize */
	packet->data = new_mem;
	packet->read_ptr = packet->data + read_offset;
	packet->allocated = new_alloc;

	return TRUE;
}

// append data to packet, resizing if necessary, moving to next position
static int packet_write(FSTPacket *packet, void* data, size_t size)
{
	if (!packet_resize (packet, packet->used + size))
		return FALSE;

	memcpy(packet->data + packet->used, data, size);
	packet->used += size;

	return TRUE;
}

// read data from packet, moving to next position
static int packet_read(FSTPacket *packet, void* data, size_t size)
{
	if(fst_packet_remaining(packet) < size)
		return FALSE;

	memcpy(data, packet->read_ptr, size);
	packet->read_ptr += size;

	return TRUE;
}


