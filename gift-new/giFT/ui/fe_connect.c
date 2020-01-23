/*
 * fe_connect.c - non-blocking socket connection wrapper...needs work
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

#include "gift-fe.h"

#include "network.h"

#include "fe_connect.h"

/*****************************************************************************/

int fe_connect_close (FEConnection *c)
{
	if (!c)
		return FALSE;

	gdk_input_remove (c->input);
	net_close (c->fd);

	free (c);

	return TRUE;
}

/*****************************************************************************/

/* wrap the connection for ease of interface */
static void connect_callback (FEConnection *c, int source,
                              GdkInputCondition cond)
{
	int ret;

	assert (c);

	ret = (*c->callback) (source, c);

	/* interface has requested that this socket is no longer required, clean it
	 * up */
	if (!ret)
	{
		gdk_input_remove (c->input);
		net_close (source);
		free (c);
	}
}

static void connect_verify (FEConnection *c, int source,
                            GdkInputCondition cond)
{
	assert (c);

	gdk_input_remove (c->input);

	/* error has occurred on the socket */
	if (net_sock_error (c->fd))
		c->fd = -1;

	if (c->data_cb)
		(*c->data_cb) (c->fd, c);

	if (c->fd < 0)
	{
		/* notify the caller that this connection is not complete */
		(*c->callback) (c->fd, c);
		free (c);

		return;
	}

	/* hooray!  create the connection wrapper */
	c->input = gdk_input_add (source, c->condition,
	                          (GdkInputFunction) connect_callback, c);
}

FEConnection *fe_connect_dispatch (char *ip, unsigned short port,
                                   FEConnectionCallback cb,
                                   GdkInputCondition cond,
								   FEConnectionCallback data_cb, void *udata)
{
	FEConnection *c;
	int fd;

	assert (ip);

	if ((fd = net_connect (ip, port, FALSE)) < 0)
		return NULL;

	c = malloc (sizeof (FEConnection));
	assert (c);

	c->fd        = fd;
	c->callback  = cb;
	c->condition = cond;
	c->data_cb   = data_cb;
	c->data      = udata;

	/* use this to check if the connection truly succeeded connection before
	 * we call the supplied cb */
	c->input = gdk_input_add (fd, GDK_INPUT_WRITE,
	                          (GdkInputFunction) connect_verify, c);

	return c;
}
