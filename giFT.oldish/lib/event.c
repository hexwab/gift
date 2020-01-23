/*
 * event.c
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

#include <errno.h>

#include "gift.h"

#include "network.h"
#include "event.h"
#include "protocol.h"
#include "connection.h"

/*****************************************************************************/

#define VALIDATE_TIMEOUT_TIMER (1 * MINUTES)

/*****************************************************************************/

/* TODO - optimize input lookup with a dataset */

#define TIME_LT  <
#define TIME_GT  >
#define TIME_EQ  ==

/* most of these are blatenly ripped from sys/time.h */
#define TIME_CMP(t,v,__OP__)                                \
	(((t)->tv_sec == (v)->tv_sec) ?                         \
	 ((t)->tv_usec __OP__ (v)->tv_usec) :                   \
	 ((t)->tv_sec __OP__ (v)->tv_sec))

#define TIME_ADD(t,v,r)                                     \
	do                                                      \
	{                                                       \
		(r)->tv_sec  = (t)->tv_sec + (v)->tv_sec;           \
		(r)->tv_usec = (t)->tv_usec + (v)->tv_usec;         \
		if ((r)->tv_usec >= 1000000)                        \
		{                                                   \
			++(r)->tv_sec;                                  \
			(r)->tv_usec -= 1000000;                        \
		}                                                   \
	} while (0)

#define TIME_ADDI(t,i,r)                                    \
	do                                                      \
	{                                                       \
		(r)->tv_sec  = (t)->tv_sec;                         \
		(r)->tv_usec = (t)->tv_usec + (i);                  \
		if ((r)->tv_usec >= 1000000)                        \
		{                                                   \
			(r)->tv_sec += (r)->tv_usec / 1000000;          \
			(r)->tv_usec %= 1000000;                        \
		}                                                   \
	} while (0)

#define TIME_SUB(t, v, r)                                   \
	do                                                      \
	{                                                       \
		(r)->tv_sec = (t)->tv_sec - (v)->tv_sec;            \
		(r)->tv_usec = (t)->tv_usec - (v)->tv_usec;         \
		if ((r)->tv_usec < 0) {                             \
			--(r)->tv_sec;                                  \
			(r)->tv_usec += 1000000;                        \
		}                                                   \
	} while (0)

#define TIME_CLEAR(t) ((t)->tv_sec = (t)->tv_usec = 0)
#define TIME_ISSET(t) ((t)->tv_sec ? 1 : (t)->tv_usec)

/*****************************************************************************/

static int loop_running = 1;

static unsigned long timer_counter = 0;

static ListLock *inputs = NULL;
static ListLock *timers = NULL;

/* it isnt safe to free input/timers at any time...so add them to this list to
 * be freed when it is safe */
static List *inputs_free = NULL;
static List *timers_free = NULL;

/*****************************************************************************/

void event_quit (int dummy)
{
	loop_running = 0;
}

/*****************************************************************************/

static int calc_next_timer (Timer *timer, struct timeval *lowest_time)
{
	if (!TIME_ISSET (lowest_time) ||
	    TIME_CMP (&timer->expiration, lowest_time, TIME_LT))
	{
		*lowest_time = timer->expiration;
	}

	return TRUE;
}

static struct timeval next_timer (struct timeval *current_time)
{
	struct timeval lowest_time = { 0, 0 };

	if (!timers)
		return lowest_time;

	/* find the lowest expiration */
	list_foreach (timers->list, (ListForeachFunc) calc_next_timer,
	              &lowest_time);

	if (TIME_CMP (&lowest_time, current_time, TIME_LT))
	{
		TIME_CLEAR (&lowest_time);
		return lowest_time;
	}

	TIME_SUB (&lowest_time, current_time, &lowest_time);
	return lowest_time;
}

static void dispatch_timers (struct timeval *current_time)
{
	List *ptr;

	if (!timers)
		return;

	list_lock (timers);

	for (ptr = timers->list; ptr; ptr = ptr->next)
	{
		Timer *timer = ptr->data;
		int    ret;

		if (TIME_CMP (&timer->expiration, current_time, TIME_GT))
			continue;

		/* this timer is ready */
		ret = (*timer->callback) (timer->data);

		if (!ret)
			timer_remove (timer->id);
		else
		{
			/* set this timer's next expiration */
			TIME_ADD (current_time, &timer->interval, &timer->expiration);
		}
	}

	list_unlock (timers);
}

/*****************************************************************************/

static int prepare_fds (fd_set *rset, fd_set *wset, fd_set *xset)
{
	List *ptr;
	int maxfd = -1;

	FD_ZERO (rset);
	FD_ZERO (wset);
	FD_ZERO (xset);

	if (!inputs)
		return maxfd;

	for (ptr = inputs->list; ptr; ptr = ptr->next)
	{
		Input *input = ptr->data;

		if (input->suspended)
			continue;

		/* calculate maxfd for select () */
		if (input->fd > maxfd)
			maxfd = input->fd;

		/* fill fd sets */
		if (input->state & INPUT_READ)
			FD_SET (input->fd, rset);

		if (input->state & INPUT_WRITE)
			FD_SET (input->fd, wset);

		if (input->state & INPUT_EXCEPTION)
			FD_SET (input->fd, xset);
	}

	return maxfd;
}

static void dispatch_input (Input *input, InputState state, fd_set *set)
{
	if (input->state & state && FD_ISSET (input->fd, set))
	{
		if (!input->complete)
		{
			input->complete = TRUE;
			timer_remove (input->validate);
		}

		(*input->callback) (input->protocol, input->data);
	}
}

static int safe_input (Input *input)
{
	List  *ptr;
	Input *ptr_input;

	assert (input != NULL);

	/* this is tricky, multiple inputs may be selected on the same fd,
	 * however these routines may have freed the data associated w/ that
	 * fd.  inputs_free will pick this up and allow us a safe way to check
	 * if this input's data has already expired */
	for (ptr = inputs_free; ptr; ptr = ptr->next)
	{
		ptr_input = ptr->data;

		/* not safe */
		if (input->fd == ptr_input->fd)
			return FALSE;
	}

	return TRUE;
}

static void dispatch_inputs (fd_set *rset, fd_set *wset, fd_set *xset)
{
	List *ptr;

	if (!inputs)
		return;

	/* these routines will fuck with the inputs list, so dont use it for
	 * processing */
	list_lock (inputs);

	for (ptr = inputs->list; ptr; ptr = ptr->next)
	{
		Input *input = ptr->data;

		if (!safe_input (input))
			continue;

		dispatch_input (input, INPUT_READ,      rset);
		dispatch_input (input, INPUT_WRITE,     wset);
		dispatch_input (input, INPUT_EXCEPTION, xset);
	}

	list_unlock (inputs);
}

/*****************************************************************************/

#if 0
static void dump_events ()
{
	List *ptr;

	for (ptr = inputs; ptr; ptr = list_next (ptr))
	{
		Input *input = ptr->data;
		struct stat st;
		char *s_error = NULL;

		if (fstat (input->fd, &st))
			s_error = GIFT_STRERROR ();

		GIFT_DEBUG (("%-4i: %-2s %-2s %-2s: %s\n", input->fd,
		        (input->state & INPUT_READ)      ? "RD" : "",
		        (input->state & INPUT_WRITE)     ? "WR" : "",
		        (input->state & INPUT_EXCEPTION) ? "EX" : "",
		        s_error ? s_error : ""));
	}
}
#endif

/*****************************************************************************/

static void cleanup (List **source_list)
{
	List *ptr, *next;

	if (!source_list || !(*source_list))
		return;

	ptr = *source_list;
	while (ptr)
	{
		next = ptr->next;

		free (ptr->data);
		free (ptr);

		ptr = next;
	}

	*source_list = NULL;
}

void event_loop ()
{
	fd_set rset, wset, xset;
	int maxfd;
	struct timeval next_event;
	struct timeval current_time;

	while (loop_running)
	{
		platform_gettimeofday (&current_time, NULL);

		next_event = next_timer (&current_time);
		maxfd = prepare_fds (&rset, &wset, &xset);

		if (select (maxfd + 1, &rset, &wset, &xset,
		            timers ? &next_event : NULL) < 0)
		{
			/* probably interrupted system call... */
			GIFT_ERROR (("select: %s", GIFT_STRERROR ()));
			continue;
		}

		/* dispatch */
		dispatch_inputs (&rset, &wset, &xset);
		dispatch_timers (&current_time);

		cleanup (&inputs_free);
		cleanup (&timers_free);
	}

	list_lock_free (inputs);
	list_lock_free (timers);

	inputs = NULL;
	timers = NULL;
}

/*****************************************************************************/
/* timer interface routines + helpers */

static int timer_find_func (Timer *timer, unsigned long *id)
{
	if (timer->id < *id)
		return -1;

	if (timer->id > *id)
		return 1;

	/* id matched, check if this timer is being freed right now or not */
	if (list_find (timers_free, timer))
		return -1;

	return 0;
}

static Timer *timer_find (unsigned long id)
{
	List *node;

	if (!timers)
		return NULL;

	node = list_find_custom (timers->list, &id, (CompareFunc) timer_find_func);

	return (node ? node->data : NULL);
}

static unsigned long timer_uniq_id ()
{
    timer_counter++;

	while (!timer_counter || timer_find (timer_counter))
		timer_counter++;

	return timer_counter;
}

/* interval is now passed in milliseconds */
unsigned long timer_add (time_t interval, TimerCallback callback, void *data)
{
	Timer *timer;
	struct timeval current_time;
	unsigned long uniq_id;

	if (!(uniq_id = timer_uniq_id ()))
		return uniq_id;

	timer = malloc (sizeof (Timer));

	timer->id         = uniq_id;

	TIME_CLEAR (&timer->interval);
	TIME_ADDI (&timer->interval, interval * 1000, &timer->interval);

	platform_gettimeofday (&current_time, NULL);
	TIME_ADD (&current_time, &timer->interval, &timer->expiration);

	timer->callback   = callback;
	timer->data       = data;

	if (!timers)
		timers = list_lock_new ();

	list_lock_prepend (timers, timer);

	return uniq_id;
}

void timer_reset (unsigned long id)
{
	Timer *timer;
	struct timeval current_time;

	if (!id)
		return;

	if (!(timer = timer_find (id)))
		return;

	/* recalculate the new expiration */
	platform_gettimeofday (&current_time, NULL);
	TIME_ADD (&current_time, &timer->interval, &timer->expiration);
}

void timer_remove (unsigned long id)
{
	Timer *timer;

	if (!id)
		return;

	if (!(timer = timer_find (id)))
		return;

	list_lock_remove (timers, timer);
	timers_free = list_prepend (timers_free, timer);
}

/*****************************************************************************/
/* input interface routines + helpers */

static int input_match (Input *input, Protocol *p, int fd,
                        InputState state, InputCallback cb)
{
	assert (input != NULL);

	if (p && input->protocol != p)
		return FALSE;

	if (cb && input->callback != cb)
		return FALSE;

	if (state && input->state != state)
		return FALSE;

	if (fd != -1 && input->fd != fd)
		return FALSE;

	/* found a match, check to see if it's already pending removal */
	if (inputs && list_find (inputs->lock_remove, input))
		return FALSE;

	/* it's an active match, give it back */
	return TRUE;
}

static Input *find_list (List **list, Protocol *p, int fd,
                         InputState state, InputCallback cb)
{
	List *ptr;

	if (!list)
		return NULL;

	for (ptr = *list; ptr; ptr = ptr->next)
	{
		if (!input_match (ptr->data, p, fd, state, cb))
			continue;

		return ptr->data;
	}

	return NULL;
}

static Input *input_find (Protocol *p, int fd,
                          InputState state, InputCallback cb)
{
	Input *input;

	if (!inputs)
		return NULL;

	/* first check the main inputs list */
	if ((input = find_list (&inputs->list, p, fd, state, cb)))
		return input;

	/* now check pending addition */
	if ((input = find_list (&inputs->lock_append, p, fd, state, cb)))
		return input;

	if ((input = find_list (&inputs->lock_prepend, p, fd, state, cb)))
		return input;

	/* it's nowhere to be found :( */
	return NULL;
}

static int validate_timeout (Input *input)
{
	/* how the hell did this happen? :P */
	if (!list_find (inputs->list, input) || input->complete)
	{
		TRACE (("%p: fixme", input));
		return FALSE;
	}

	/* this is kinda sloppy, oh well */
	net_close (input->fd);

	/* input->callback may remove it from the select loop, but we really can't
	 * be sure of that...so check here */
	input_remove (input->data);

	/* notify the input holder that it timed out ... it will be passed an
	 * invalid socket (via input->data), hopefully it can handle that */
	(*input->callback) (input->protocol, input->data);

	return FALSE;
}

void input_add (Protocol *p, Connection *c, InputState state,
                InputCallback callback, int timeout)
{
	Input *input;

	if (!c || c->fd < 0)
		return;

	input = malloc (sizeof (Input));

	input->fd        = c->fd;
	input->state     = state;
	input->callback  = callback;
	input->protocol  = p;        /* tracking */
	input->data      = c;
	input->suspended = 0;

	/* this input will timeout if not set completed within
	 * 1 minute (sanity check) */
	if (!timeout)
	{
		input->complete = TRUE;
		input->validate = 0;
	}
	else
	{
		input->complete = FALSE;
		input->validate =
		    timer_add (VALIDATE_TIMEOUT_TIMER,
		               (TimerCallback) validate_timeout, input);
	}

	if (!inputs)
		inputs = list_lock_new ();

	list_lock_prepend (inputs, input);
}

void input_remove_full (Protocol *p, Connection *c,
                        InputState state, InputCallback cb)
{
	Input *input;
	int removed = 0;

	if (!p && (!c || c->fd < 0))
		return;

	while ((input = input_find (p, c ? c->fd : -1, state, cb)))
	{
		list_lock_remove (inputs, input);
		inputs_free = list_prepend (inputs_free, input);

		timer_remove (input->validate);
		input->validate = 0;

		removed++;
	}

	/* return removed; */
}

void input_remove (Connection *c)
{
	input_remove_full (NULL, c, 0, NULL);
}

void input_suspend (Connection *c)
{
	Input *input;

	if (!(input = input_find (NULL, c ? c->fd : -1, 0, NULL)))
		return;

	input->suspended = TRUE;
}

void input_resume (Connection *c)
{
	Input *input;

	if (!(input = input_find (NULL, c ? c->fd : -1, 0, NULL)))
		return;

	input->suspended = FALSE;
}
