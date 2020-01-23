/*
 * interface.h
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

#ifndef __INTERFACE_H
#define __INTERFACE_H

/*****************************************************************************/

/**
 * @file interface.h
 *
 * @brief Low-level interface protocol manipulation routines.
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

typedef struct
{
	char *command;
	char *value;
	Tree *tree;
} Interface;

typedef void (*InterfaceForeach) (Interface *p, char *keypath, int children,
                                  char *key, char *value, void *udata);

/*****************************************************************************/

Interface *interface_new (char *command, char *value);
void interface_free (Interface *p);

void interface_set_command (Interface *p, char *command);
void interface_set_value (Interface *p, char *value);

char *interface_get (Interface *p, char *key);
#define INTERFACE_GETLU(p,k) ((unsigned long)ATOUL(interface_get(p,k)))
#define INTERFACE_GETLI(p,k) ((signed long)ATOUL(interface_get(p,k)))
#define INTERFACE_GETL(p,k) INTERFACE_GETLI(p,k)
#define INTERFACE_GETI(p,k) ((int)(ATOI(interface_get(p,k))))
#define INTERFACE_GETU(p,k) ((unsigned int)(ATOI(interface_get(p,k))))

int interface_put (Interface *p, char *key, char *value);
#define INTERFACE_PUTLU(p,k,v) interface_put(p, k, stringf ("%lu", (unsigned long)v))
#define INTERFACE_PUTLI(p,k,v) interface_put(p, k, stringf ("%li", (long)v))
#define INTERFACE_PUTL(p,k,v) INTERFACE_PUTLI(p,k,v)
#define INTERFACE_PUTI(p,k,v) interface_put(p, k, stringf ("%i", (int)v))
#define INTERFACE_PUTU(p,k,v) interface_put(p, k, stringf ("%u", (unsigned int)v))

void interface_foreach (Interface *p, char *key, InterfaceForeach func,
						void *udata);

String *interface_serialize (Interface *p);
Interface *interface_unserialize (char *data, size_t len);

int interface_send (Interface *p, Connection *c);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __INTERFACE_H */
