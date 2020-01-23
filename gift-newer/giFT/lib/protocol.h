/*
 * protocol.h
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

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

/*****************************************************************************/

struct _list;
struct _hash_table;
struct _file_share;
struct _protocol;
struct _connection;
struct _chunk;

/*****************************************************************************/

typedef int (*ProtocolInit) (struct _protocol *p);

/* TODO -- merge ShareCommand in here */
typedef enum
{
	PROTOCOL_SHARE_ADD = 0,
	PROTOCOL_SHARE_REMOVE,
	PROTOCOL_SHARE_FLUSH,        /* remove all shares with your server */
	PROTOCOL_SHARE_SYNC,         /* add them all again */
	PROTOCOL_TRANSFER_START,
	PROTOCOL_TRANSFER_CANCEL,
	PROTOCOL_CHUNK_SUSPEND,
	PROTOCOL_CHUNK_RESUME,
} ProtocolCommand;

/*****************************************************************************/

typedef struct _protocol
{
	/* GENERAL INFORMATION */

	char               *name;
	void               *handle;

	/* PROTOCOL-SPECIFIC */

	struct _hash_table *support; /* protocol-specific features */

	void               *data;    /* arbitrary data associated with this
	                              * protocol */

	/* CALLBACKS */

	/* <search query=.../> */
    void (*callback) (struct _protocol *p, struct _connection *c,
	                  Dataset *event);

	/* explicit transfer requests */
    void (*download) (struct _chunk *chunk, ProtocolCommand command,
	                  void *data);
	void (*upload)   (struct _chunk *chunk, ProtocolCommand command,
	                  void *data);

	/* sharing needs to be flushed or resynched w/ the protocol */
	void (*share)    (struct _file_share *file, ProtocolCommand command);

	/* UI <-> daemon connection closed */
	void (*dc_close) (struct _connection *c);

	/* protocol was unloaded */
	void (*destroy)  (struct _protocol *p);
} Protocol;

/*****************************************************************************/

Protocol *protocol_new     ();
void      protocol_destroy (Protocol *p);

List     *protocol_list    ();
Protocol *protocol_find    (char *name);
void      protocol_add     (Protocol *p);
void      protocol_remove  (Protocol *p);

/*****************************************************************************/

#endif /* __PROTOCOL_H */
