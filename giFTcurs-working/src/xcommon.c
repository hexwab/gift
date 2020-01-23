/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001-2002 Göran Weinholt <weinholt@linux.nu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: xcommon.c,v 1.54 2002/10/22 13:36:46 weinholt Exp $
 */
#include "giftcurs.h"

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>

#include "xcommon.h"
#include "screen.h"
#include "parse.h"

#ifdef HAVE_GETADDRINFO
int xconnect(char *hostname, char *port)
{
	struct addrinfo *res, *res0, hints = { 0, AF_UNSPEC, SOCK_STREAM };
	int sock = -1, e;

	if ((e = getaddrinfo(hostname, port, &hints, &res0)))
		DIE("getaddrinfo %s:%s: %s (%d)", hostname, port, gai_strerror(e), e);

	for (res = res0; res; res = res->ai_next) {
		sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sock < 0)
			continue;

		if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
			close(sock);
			sock = -1;
			continue;
		}
		break;
	}
	freeaddrinfo(res0);

	if (sock < 0) {
		ERROR("socket||connect");
		return -1;
	}

	return sock;
}
#else
int xconnect(char *hostname, char *port)
{
	int sock;

	if (hostname[0] == '/') {
		/* This is a unix domain socket. */
		struct sockaddr_un saddr;

		if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
			FATAL("socket");

		saddr.sun_family = AF_UNIX;
		strncpy(saddr.sun_path, hostname, sizeof saddr.sun_path);
		if (connect(sock, (struct sockaddr *) &saddr, sizeof saddr) < 0) {
			ERROR("connect");
			close(sock);
			return -1;
		}
	} else {
		struct hostent *host;
		struct sockaddr_in saddr;
		struct servent *serv;

		/* This function fails in cygwin and on Tru64. */
		if ((serv = getservbyname(port, NULL)))
			saddr.sin_port = serv->s_port;  /* Already in network byteorder */
		else
			saddr.sin_port = htons(atoi(port));

		if (!(host = gethostbyname(hostname)))
			DIE("%s: %s (%d)", hostname, hstrerror(h_errno), h_errno);

		memcpy(&saddr.sin_addr.s_addr, host->h_addr_list[0], host->h_length);
		saddr.sin_family = AF_INET;

		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			FATAL("socket");

		if (connect(sock, (struct sockaddr *) &saddr, sizeof saddr) < 0) {
			ERROR("connect");
			close(sock);
			return -1;
		}
	}

	return sock;
}
#endif

/* xgetlines reads chunks from a socket, and calls a handle for each chunk read.
 * The mysterious argument separator is a function which, given the buffer,
 * should tell if there is a chunk sepearator in it, and in such case return
 * a pointer to it. For linereading this can be "strchr(buf, '\n')"
 * The separator is removed before calling handle with the chunk.
 * -- (well, better than nothing, I hope...)
 */
int xgetlines(int fd, buffer * buf, int (*handle) (char *buf), char *(*separator) (char *))
{
	int n;
	char *p;

	if (!buf->buf) {
		buf->alloc = 8192;
		if (!(buf->buf = malloc(buf->alloc)))
			FATAL("malloc");
		buf->len = 0;
	} else if (buf->len + 4096 > buf->alloc) {
		buf->alloc *= 2;
		if (!(buf->buf = realloc(buf->buf, buf->alloc)))
			FATAL("realloc");
	}

	p = buf->buf;

	n = read(fd, p + buf->len, buf->alloc - buf->len - 1);
	if (n < 0) {
		ERROR("read");
		buf->len = 0;
		return -1;
	}
	if (n == 0) {
		/* EOF, Throw away the data we have */
		if (buf->len) {
			p[buf->len] = '\0';
			DEBUG("Incomplete last line on fd %d (%s)", fd, p);
			buf->len = 0;
		}
		return 0;
	}

	if (memchr(p + buf->len, '\0', n)) {
		DEBUG("fd %d is sending me NULs.", fd);
		buf->len = 0;
		return 1;
	}

	buf->len += n;
	p[buf->len] = '\0';

	assert(strlen(p) == buf->len);

	for (;;) {
		char *next = separator(p);

		if (!next) {
			memmove(buf->buf, p, buf->len);
			return n;
		}
		*next = '\0';
		if (handle(p) < 0) {
			buf->len = 0;
			return -1;
		}
		buf->len -= next - p + 1;
		if (!buf->len)
			return n;
		assert(buf->len > 0);
		p = next + 1;
	}
}

/* Note, this doesn't append a newline like puts() does. */
void xputs(const char *s, int fd)
{
	int len = strlen(s);
	int n = write(fd, s, len);

	if (n != len) {
		if (n < 0)
			ERROR("write");
		else
			DEBUG("Partial write! %d bytes lost.", len - n);
	}
}
