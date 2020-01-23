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
 * $Id: poll.c,v 1.55 2002/09/20 22:32:58 chnix Exp $
 */
#include "giftcurs.h"

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>

#ifdef HAVE_POLL
# include <sys/poll.h>
#else
# include <sys/select.h>
#endif

#include "poll.h"
#include "screen.h"
#include "list.h"

#define MAX_FDS 4

void (*ugly_hack) (int) = NULL;

struct timer {
	tick_t at;
	TFunc fn;
	void *data;
};

static void poll_once(void);

/* Filedescriptors to be monitored are stored in two parallel arrays */
#ifdef HAVE_POLL
static struct pollfd pfds[MAX_FDS];
#else
static struct {
	int fd;
} pfds[MAX_FDS];
#endif
static FFunc fd_handlers[MAX_FDS];
static int nfds = 0;

/* Timers are stored in a sorted list */
static list timers;

/* Timekeeping stuff */
static tick_t uptime_cached = 0;
static time_t basetime = 0;

int poll_add_timer(unsigned int delay, TFunc fn, void *data)
{
	struct timer *new = malloc(sizeof *new);

	new->at = uptime() + delay;
	new->fn = fn;
	new->data = data;
	list_insort(&timers, new);
	return 0;
}

int poll_del_timer(TFunc fn)
{
	int i;

	for (i = 0; i < timers.num; i++) {
		struct timer *t = list_index(&timers, i);

		if (t->fn == fn) {
			free(t);
			list_remove_entry(&timers, i);
			return 0;
		}
	}
	return -1;
}

int poll_add_fd(int fd, FFunc fn)
{
	int i;

	assert(fn != NULL);
	assert(nfds < MAX_FDS);

	for (i = 0; i < nfds; i++)
		assert(pfds[i].fd != fd);
	pfds[nfds].fd = fd;
#ifdef HAVE_POLL
	pfds[nfds].events = POLLIN;
#endif
	fd_handlers[nfds++] = fn;
	return 0;
}

int poll_del_fd(int fd)
{
	int i;

	for (i = 0; pfds[i].fd != fd; i++)
		if (i >= nfds)
			return -1;
	pfds[i] = pfds[--nfds];
	fd_handlers[i] = fd_handlers[nfds];
	return 0;
}

void timer_destroy(void)
{
	assert(nfds == 0);
	assert(timers.num == 0);
}

void poll_forever(void)
{
	while (nfds || timers.num)
		poll_once();
}

/* Wait for an event to occur and handle it */
static void poll_once(void)
{
	int fd, i, timeout = -1, poll_ret;
	struct timer *t = NULL;

#ifdef HAVE_POLL
	int ev;
#else
	int max = 0;
	fd_set set;
	struct timeval tv, *tvp = NULL;
#endif

	if (ugly_hack)
		ugly_hack(0);

	if (timers.num) {
		t = list_index(&timers, 0);

		timeout = t->at - uptime();
		if (timeout <= 0)
			goto dispatch_timer;
		timeout *= 10;			/* convert from centiseconds to milliseconds */
	}

	/* This is the only place where we sleep - invalidate uptime cache */
	uptime_cached = 0;

#ifdef HAVE_POLL
	poll_ret = poll(pfds, nfds, timeout);
#else
	if (timeout != -1) {
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		tvp = &tv;
	}
	FD_ZERO(&set);
	for (i = 0; i < nfds; i++) {
		if (pfds[i].fd > max)
			max = pfds[i].fd;
		FD_SET(pfds[i].fd, &set);
	}
	poll_ret = select(max + 1, &set, NULL, NULL, tvp);
#endif

	switch (poll_ret) {
	case -1:
		if (errno != EINTR)
			ERROR("poll");
		return;
	case 0:
	  dispatch_timer:
		list_remove_entry(&timers, 0);

		timeout = t->fn(t->data);

		if (timeout) {
			t->at = uptime() + timeout;
			list_insort(&timers, t);
		} else
			free(t);
		return;
	default:
		/* Find the first guilty */
#ifdef HAVE_POLL
		for (i = 0; (ev = pfds[i].revents) == 0; i++)
			assert(i < nfds);

		/* check for errors we did */
		assert(!(ev & (POLLNVAL | POLLOUT)));
#else
		for (i = 0; !FD_ISSET(pfds[i].fd, &set); i++)
			assert(i < nfds);
#endif
		fd = pfds[i].fd;

		fd_handlers[i] (fd);
		return;
	}
}

/* calculates number of ticks since program start */
tick_t uptime(void)
{
	struct timeval tv;

	/* caching of uptime value cuts number of gettimeofday calls by half */
	if (uptime_cached)
		return uptime_cached;

	assert(basetime);
	gettimeofday(&tv, NULL);
	/* read the time in centiseconds */
	uptime_cached = (tv.tv_sec - basetime) * 100 + tv.tv_usec / 10000;
	return uptime_cached;
}

static int compare_timers(struct timer *a, struct timer *b)
{
	return a->at - b->at;
}

void timer_init(void)
{
	time(&basetime);
	list_sort(&timers, (CmpFunc) compare_timers);
}
