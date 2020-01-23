/*
 * fe_connect.h
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

#ifndef __FE_CONNECT_H
#define __FE_CONNECT_H

/**************************************************************************/

struct _fe_connection;

typedef int (*FEConnectionCallback) (int sock, struct _fe_connection *c);

typedef struct _fe_connection {
	int fd;
	int input;
	int attached;
	FEConnectionCallback callback;
	GdkInputCondition condition;

	/* user data */
	FEConnectionCallback data_cb;
	void *data;
} FEConnection;

/**************************************************************************/

int           fe_connect_close    (FEConnection *c);
FEConnection *fe_connect_dispatch (char *ip, unsigned short port,
                                   FEConnectionCallback cb,
                                   GdkInputCondition cond,
								   FEConnectionCallback data_cb, void *udata);

/**************************************************************************/

#endif /* __FE_CONNECT_H */
