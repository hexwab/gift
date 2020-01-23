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

#include "gift.h"

#include "network.h"

#ifndef WIN32
# include <errno.h>
typedef void* optval_t;
#else /* WIN32 */
typedef char* optval_t;
#endif /* !WIN32 */

#ifndef SHUT_RDWR
# define SHUT_RDWR 2
#endif /* SHUT_RDWR */

static int default_buf_size = -1;

/**************************************************************************/

void net_close (int fd)
{
	if (fd < 0)
		return;

#ifndef WIN32
	shutdown (fd, SHUT_RDWR);
	close (fd);
#else /* !WIN32 */
	shutdown (fd, SD_BOTH);
	closesocket (fd);
#endif /* WIN32 */
}

int net_connect (char *ip, unsigned short port, int blocking)
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

	net_set_blocking (s_fd, blocking);

	if (connect (s_fd, (struct sockaddr *)&server, sizeof (struct sockaddr)) < 0 &&
#ifndef WIN32
	    errno != EINPROGRESS)
#else /* WIN32 */
	    WSAGetLastError () != WSAEWOULDBLOCK)
#endif /* !WIN32 */
	{
		net_close (s_fd);
		return -1;
	}

	return s_fd;
}

int net_accept (int s_fd, int blocking)
{
	int accept_sock, len;
	struct sockaddr_in saddr;

	len = sizeof (saddr);

	accept_sock = accept (s_fd, (struct sockaddr *)&saddr, &len);

	net_set_blocking (accept_sock, blocking);

	return accept_sock;
}

int net_bind (unsigned short port, int blocking)
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

	setsockopt (s_fd, SOL_SOCKET, SO_REUSEADDR, (optval_t) &len, sizeof (len));

	net_set_blocking (s_fd, blocking);

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
#ifndef WIN32
	int flags;

	flags = fcntl (s_fd, F_GETFL);

	if (blocking)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;

	fcntl (s_fd, F_SETFL, flags);

	return flags;
#else /* WIN32 */
	unsigned long arg = !blocking;

	ioctlsocket (s_fd, FIONBIO, &arg);

	return arg;
#endif /* !WIN32 */
}

/*****************************************************************************/

int net_sock_error (int s_fd)
{
	int err, len, x;

	len = sizeof (err);

	x = getsockopt (s_fd, SOL_SOCKET, SO_ERROR, (optval_t) &err, &len);

	return (x < 0 || err);
}

int net_sock_adj_buf (int s_fd, int buf_name, float factor)
{
	int buf_size;
	int len;

	if (s_fd < 0)
		return -1;

	len = sizeof (buf_size);

	if (default_buf_size == -1)
	{
		if (getsockopt (s_fd, SOL_SOCKET, buf_name,
		                (optval_t) &default_buf_size, &len) < 0)
		{
			default_buf_size = RW_SOCKBUFFER;
		}
	}

	buf_size = CLAMP ((default_buf_size * factor),
	                  0, default_buf_size);

	if (buf_size < 0)
		return -1;

	/* don't error check these, it's just going to annoy people ;) */
	setsockopt (s_fd, SOL_SOCKET, buf_name, (optval_t) &buf_size, len);

#if 0
	/* check to make sure it succeeded */
	getsockopt (s_fd, SOL_SOCKET, buf_name, (optval_t) &buf_size, &len);
#endif

	return buf_size;
}

/*****************************************************************************/

int net_send (int fd, char *data, size_t len)
{
	int flags = 0;

	if (len == 0)
		len = strlen (data);

	/* debugging simplicity ... SIGPIPE is still SIG_IGN */
#ifdef MSG_NOSIGNAL
	flags |= MSG_NOSIGNAL;
#endif

	return send (fd, (void *) data, len, flags);
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

/* returns a netmask given the desired bitwidth */
unsigned long net_mask (int bitwidth)
{
	unsigned long mask;

	for (mask = 0; bitwidth > 0; bitwidth--)
	{
		/* TODO - hardcoding size is evil */
		mask |= (1 << (32 - bitwidth));
	}

	mask = htonl (mask);

	return mask;
}

/*****************************************************************************/

/* determine the locally bound ip address */
unsigned long net_local_ip ()
{
	/* TODO */
	return inet_addr ("127.0.0.1");
}

/*****************************************************************************/

/* match may be in the form 192.168.0.0/16 or "LOCAL" */
int net_match_host (unsigned long ip, char *match)
{
	char *host;
	char *block;
	unsigned short bitwidth;
	unsigned long  host_ip = 0;

	/* this is a bad idea */
	if (!strcasecmp (match, "ALL"))
		return TRUE;

	/* special case to match all local addresses */
	if (!strcasecmp (match, "LOCAL"))
	{
		/* 0.0.0.0 */
		if (!ip)
			return TRUE;

		/* convert to host order to make things easier and more efficient
		 * below */
		ip = ntohl (ip);

		if (((ip & 0xff000000) == 0x7f000000) || /* 127.0.0.0 */
		    ((ip & 0xffff0000) == 0xc0a80000) || /* 192.168.0.0 */
		    ((ip & 0xffe00000) == 0xac100000) || /* 172.16-31.0.0 */
		    ((ip & 0xff000000) == 0x0a000000))   /* 10.0.0.0 */
		{
			return TRUE;
		}

		return FALSE;
	}

	/* we need to parse 152.32.0.0[/16] */
	host = string_sep (&match, "/");
	bitwidth = (match && *match) ? ATOI (match) : 32;

	/* TODO - this won't work with incomplete IP addresses */
	while ((block = string_sep (&host, ".")))
	{
		host_ip = (host_ip << 8 | (ATOI (block) & 0xff));
	}

	/* convert to network order */
	host_ip = htonl (host_ip);

	/* apply the netmask */
	ip &= net_mask (bitwidth);

	/* compare */
	return (ip == host_ip);
}
