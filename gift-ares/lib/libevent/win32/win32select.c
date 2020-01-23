/*	$OpenBSD: select.c,v 1.2 2002/06/25 15:50:15 mickey Exp $	*/

/*
 * Copyright 2000-2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define FD_SETSIZE 1024
#include <winsock2.h>

#include <sys/types.h>
#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <assert.h>

#ifdef USE_LOG
#include "log.h"
#else
#define LOG_DBG(x)
#define log_error(x)	perror(x)
#endif

#include "event.h"

extern struct event_list eventqueue;

struct win32selectop {
	unsigned int fd_count;
	fd_set event_readset;
	fd_set event_writeset;
	fd_set event_exceptset;
} sop;

void *win32select_init	(void);
int win32select_add		(void *, struct event *);
int win32select_del		(void *, struct event *);
int win32select_recalc	(void *, int);
int win32select_dispatch	(void *, struct timeval *);

const struct eventop win32selectops = {
	"win32select",
	win32select_init,
	win32select_add,
	win32select_del,
	win32select_recalc,
	win32select_dispatch
};

void *
win32select_init(void)
{
	sop.fd_count = 0;

	return (&sop);
}

/*
 * Called with the highest fd that we know about.  If it is 0, completely
 * recalculate everything.
 */

int
win32select_recalc(void *arg, int max)
{
	/* FDs are not "small positive integers" on Windows. We use fixed size
	 * FD_SETSIZE arrays. */
	return 0;
}

int net_sock_error (int fd);

int
win32select_dispatch(void *arg, struct timeval *tv)
{
	int res;
	struct event *ev, *next;
	struct win32selectop *sop = arg;
	int i = 0;

	FD_ZERO(&sop->event_readset);
	FD_ZERO(&sop->event_writeset);
	FD_ZERO(&sop->event_exceptset);

	/* Add fds to sets */
	TAILQ_FOREACH(ev, &eventqueue, ev_next) {
		if (ev->ev_events & EV_WRITE)
			FD_SET((SOCKET)ev->ev_fd, &sop->event_writeset);
		if (ev->ev_events & EV_READ)
			FD_SET((SOCKET)ev->ev_fd, &sop->event_readset);

		/* POSIX specifies that a failed non-blocking connect should signal a
		 * writable socket. Windows only signals this case through the except
		 * set so we add the fd there as well if a write signal is requested. 
		 */
		if (ev->ev_events & (EV_EXCEPT | EV_WRITE))
			FD_SET((SOCKET)ev->ev_fd, &sop->event_exceptset);
	}

	if (sop->event_readset.fd_count == 0 &&
		sop->event_writeset.fd_count == 0 &&
		sop->event_exceptset.fd_count == 0)
	{
		/* just wait */
		Sleep (tv->tv_sec * 1000 + tv->tv_usec / 1000);
		return (0);
	}

	res = select (0, &sop->event_readset, &sop->event_writeset,
	              &sop->event_exceptset, tv);

	if (res == SOCKET_ERROR) {
		switch (WSAGetLastError ())
		{
		case WSAENOTSOCK:
			log_error ("win32 select called with invalid fd");
			break;
		case WSAEINVAL:
			log_error ("win32 select called with invalid parameters");
			break;
		default:
			log_error ("win32 select error");
			break;
		}
		
		return (-1);

/*
		if (errno != EINTR) {
			log_error("select");
			assert (0);
			return (-1);
		}

		return (0);
*/
	}

	LOG_DBG((LOG_MISC, 80, "%s: select reports %d", __func__, res));

	if (res == 0) {
		/* timeout, no need to check fds */
		return (0);
	}

	for (ev = TAILQ_FIRST(&eventqueue); ev != NULL; ev = next) {
		next = TAILQ_NEXT(ev, ev_next);

		res = 0;
		if (FD_ISSET(ev->ev_fd, &sop->event_readset))
			res |= EV_READ;
		if (FD_ISSET(ev->ev_fd, &sop->event_writeset))
			res |= EV_WRITE;
		if (FD_ISSET(ev->ev_fd, &sop->event_exceptset))	{
			res |= EV_EXCEPT;

			/* POSIX compatibility hack, see above. */
			if (ev->ev_events & EV_WRITE)
				res |= EV_WRITE;
		}
		res &= ev->ev_events;

		if (res) {
			if (!(ev->ev_events & EV_PERSIST))
				event_del(ev);

			event_active(ev, res, 1);
		} 
	}

	return (0);
}

int
win32select_add(void *arg, struct event *ev)
{
	struct win32selectop *sop = arg;

	if (sop->fd_count >= FD_SETSIZE)	{
		log_error("Cannot handle more than FD_SETSIZE fds");

		assert (sop->fd_count < FD_SETSIZE);
		return (-1);
	}

	sop->fd_count++;

	return (0);
}

/*
 * Nothing to be done here.
 */

int
win32select_del(void *arg, struct event *ev)
{
	struct win32selectop *sop = arg;

	assert (sop->fd_count > 0);
	sop->fd_count--;

	return (0);
}
