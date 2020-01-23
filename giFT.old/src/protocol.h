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
struct _protocol;
struct _connection;
struct _chunk;

typedef int (*ProtocolInit) (struct _protocol *p);

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
    void (*download) (struct _chunk *chunk);
	void (*upload)   (struct _chunk *chunk);

	/* UI <-> daemon connection closed */
	void (*dc_close) (struct _connection *c);

	/* protocol was unloaded */
	void (*destroy)  (struct _protocol *p);
} Protocol;

/*****************************************************************************/

/* use these instaid of malloc/free */
Protocol *protocol_new     ();
void      protocol_destroy (Protocol *p);

#if 0
/*
 * Add an event to a protocol.  The elipse should be filled with
 * char/OPT_TYPE_* pairs.
 */
Protocol *protocol_add_event (Protocol *p, char *name, int numargs, ...);
#endif

Protocol *protocol_load   (char *fname);
void      protocol_unload (Protocol *p);
Protocol *protocol_find   (char *name);

#endif /* __PROTOCOL_H */
