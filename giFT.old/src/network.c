/*
 * network.c
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

#include <stdio.h>

#ifndef WIN32
#include <errno.h>
#endif

#include "network.h"

/**************************************************************************/

void net_close (int fd)
{
	if (fd < 0)
		return;

#ifdef WIN32
	shutdown (fd, SD_BOTH);
#else
	shutdown (fd, SHUT_RDWR);
#endif

	close (fd);
}

int net_connect (char *ip, unsigned short port)
{
	int s_fd;
	struct sockaddr_in server;

	if (!ip || !port)
		return -1;

	s_fd = socket (AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (s_fd <= -1)
		return s_fd;

	memset (&server, 0, sizeof (struct sockaddr_in));
	server.sin_family      = AF_INET;
	server.sin_port        = htons (port);
	server.sin_addr.s_addr = inet_addr (ip);

	net_set_blocking (s_fd, 0);

	if (connect (s_fd, (struct sockaddr *)&server, sizeof (struct sockaddr)) < 0 &&
#ifdef WIN32
	    WSAGetLastError () != WSAEWOULDBLOCK)
#else
		errno != EINPROGRESS)
#endif
	{
		net_close (s_fd);
		return -1;
	}

	return s_fd;
}

int net_accept (int s_fd)
{
	int accept_sock, len;
	struct sockaddr_in saddr;

	len = sizeof (saddr);

	accept_sock = accept (s_fd, (struct sockaddr *)&saddr, &len);

	/* TODO - is this inherited? */
	net_set_blocking (accept_sock, 0);

	return accept_sock;
}

int net_bind (unsigned short port)
{
	int s_fd, len;
	struct sockaddr_in server;

	s_fd = socket (AF_INET, SOCK_STREAM, IPPROTO_IP);

	if (s_fd < 0)
		return s_fd;

	memset (&server, 0, sizeof (server));
	server.sin_family      = AF_INET;
	server.sin_addr.s_addr = htonl (INADDR_ANY);
	server.sin_port        = htons (port);

	/* set something non-zero */
	len = sizeof (server);

#ifdef WIN32
	setsockopt (s_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &len,
	            sizeof (len));
#else
	setsockopt (s_fd, SOL_SOCKET, SO_REUSEADDR, &len, sizeof (len));
#endif

	net_set_blocking (s_fd, 0);

	if (bind (s_fd, (struct sockaddr *)&server, sizeof (struct sockaddr)) < 0)
	{
		perror ("bind");
		net_close (s_fd);
		return -1;
	}

	listen (s_fd, 5);

	return s_fd;
}

/*****************************************************************************/

int net_set_blocking (int s_fd, int blocking)
{
#ifdef WIN32
	unsigned long arg = !blocking;

	ioctlsocket (s_fd, FIONBIO, &arg);

	return arg;
#else /* !WIN32 */
	int flags;

	flags = fcntl (s_fd, F_GETFL);

	if (blocking)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;

	fcntl (s_fd, F_SETFL, flags);

	return flags;
#endif /* WIN32 */
}

/*****************************************************************************/

int net_sock_error (int s_fd)
{
	int err, len, x;

	len = sizeof (err);

#ifdef WIN32
	x = getsockopt (s_fd, SOL_SOCKET, SO_ERROR, (char *) &err, &len);
#else
	x = getsockopt (s_fd, SOL_SOCKET, SO_ERROR, &err, &len);
#endif

	return (x < 0 || err);
}

/*****************************************************************************/

int net_send (int fd, char *data)
{
	return send (fd, data, strlen (data), 0);
}

char *net_ip_str (unsigned long ip)
{
	struct in_addr ina;

	ina.s_addr = ip;

	return inet_ntoa (ina);
}

char *net_peer_ip (int fd)
{
	struct sockaddr_in saddr;
	unsigned long ip = 0;
	int len;

	len = sizeof (saddr);
	if (fd >= 0 && getpeername (fd, (struct sockaddr *)&saddr, &len) == 0)
		ip = saddr.sin_addr.s_addr;

	return net_ip_str (ip);
}

/*****************************************************************************/

/* determine the locally bound ip address */
unsigned long net_local_ip ()
{
	/* TODO */
	return inet_addr ("127.0.0.1");
}
