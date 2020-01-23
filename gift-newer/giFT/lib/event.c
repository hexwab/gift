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

static List      *inputs = NULL;
static HashTable *timers = NULL;

/* it isnt safe to free input/timers at any time...so add them to this list to
 * be freed when it is safe */
static List *inputs_free = NULL;
static List *timers_free = NULL;

/*****************************************************************************/

void event_quit ()
{
	loop_running = 0;
}

/*****************************************************************************/

static int calc_next_timer (unsigned long key, Timer *timer,
                            struct timeval *lowest_time)
{
	if (!TIME_ISSET (lowest_time) || TIME_CMP (&timer->expiration, lowest_time, TIME_LT))
		*lowest_time = timer->expiration;

	return TRUE;
}

static struct timeval next_timer (struct timeval *current_time)
{
	struct timeval lowest_time = {0, 0};

	hash_table_foreach (timers, (HashFunc) calc_next_timer, &lowest_time);

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
	List *safe_copy, *ptr;

	/* TODO - opt */
	safe_copy = hash_flatten (timers);

	for (ptr = safe_copy; ptr; ptr = ptr->next)
	{
		Timer *timer = ptr->data;
		int ret;

		if (TIME_CMP (&timer->expiration, current_time, TIME_GT))
			continue;

		ret = (*timer->callback) (timer->data);

		if (!ret)
			timer_remove (timer->id);
		else
			TIME_ADD (current_time, &timer->interval, &timer->expiration);
	}

	list_free (safe_copy);
}

/*****************************************************************************/

static int prepare_fds (fd_set *rset, fd_set *wset, fd_set *xset)
{
	List *ptr;
	int maxfd = -1;

	FD_ZERO (rset);
	FD_ZERO (wset);
	FD_ZERO (xset);

	for (ptr = inputs; ptr; ptr = ptr->next)
	{
		Input *input = ptr->data;

		if(input->suspended)
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

static void dispatch_inputs (fd_set *rset, fd_set *wset, fd_set *xset)
{
	List *safe_copy, *ptr;


	/* these routines will fuck with the inputs list, so dont use it for
	 * processing */
	/* safe_copy = list_lock (inputs); */
	safe_copy = list_copy (inputs);

	for (ptr = safe_copy; ptr; ptr = ptr->next)
	{
		Input *input = ptr->data;
		int    safe  = TRUE;
		List  *safety;

		/* this is tricky, multiple inputs may be selected on the same fd,
		 * however these routines may have freed the data associated w/ that
		 * fd.  inputs_free will pick this up and allow us a safe way to check
		 * if this input's data has already expired */
		for (safety = inputs_free; safety; safety = safety->next)
		{
			Input *input_free = safety->data;

			if (input->fd == input_free->fd)
			{
				safe = FALSE;
				break;
			}
		}

		/* we must get out */
		if (!safe)
			continue;

		dispatch_input (input, INPUT_READ,      rset);
		dispatch_input (input, INPUT_WRITE,     wset);
		dispatch_input (input, INPUT_EXCEPTION, xset);
	}

	/* inputs = list_unlock (safe_copy); */
	list_free (safe_copy);
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
		{
			s_error = strerror (errno);
		}

		printf ("%-4i: %-2s %-2s %-2s: %s\n", input->fd,
				(input->state & INPUT_READ)      ? "RD" : "",
				(input->state & INPUT_WRITE)     ? "WR" : "",
				(input->state & INPUT_EXCEPTION) ? "EX" : "",
				s_error ? s_error : "");
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

		/* TODO - next_timer should probably have microsecond precision */
		next_event = next_timer (&current_time);
		maxfd = prepare_fds (&rset, &wset, &xset);

		if (select (maxfd + 1, &rset, &wset, &xset,
		            timers ? &next_event : NULL) < 0)
		{
			/* oh shit */
			perror ("select");
			continue;
#if 0
			dump_events ();
			abort ();
#endif
		}

		/* dispatch */
		dispatch_inputs (&rset, &wset, &xset);
		dispatch_timers (&current_time);

		cleanup (&inputs_free);
		cleanup (&timers_free);
	}
}

/*****************************************************************************/
/* timer interface routines + helpers */

/* TODO - optimize */
static unsigned long timer_uniq_id ()
{
    timer_counter++;

	if (!timer_counter)
		timer_counter++;

	while (hash_table_lookup (timers, timer_counter))
		timer_counter++;

	return timer_counter;
}

/* interval is now passed in milliseconds */
int timer_add (time_t interval, TimerCallback callback, void *data)
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
		timers = hash_table_new ();

	hash_table_insert (timers, uniq_id, timer);

	return uniq_id;
}

void timer_reset (unsigned long id)
{
	Timer *timer;
	struct timeval current_time;

	if (!id)
		return;

	if (!(timer = hash_table_lookup (timers, id)))
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

	if (!(timer = hash_table_lookup (timers, id)))
		return;

	hash_table_remove (timers, id);
	timers_free = list_prepend (timers_free, timer);
}

/*****************************************************************************/
/* input interface routines + helpers */

static Input *input_find (Protocol *p, int fd,
                          InputState state, InputCallback cb)
{
	List *ptr;

	for (ptr = inputs; ptr; ptr = ptr->next)
	{
		Input *input = ptr->data;

		if (p && input->protocol != p)
			continue;

		if (cb && input->callback != cb)
			continue;

		if (state && input->state != state)
			continue;

		if (fd != -1 && input->fd != fd)
			continue;

		/* it's a match */
		return input;
	}

	return NULL;
}

static int validate_timeout (Input *input)
{
	/* how the hell did this happen? :P */
	if (input->complete)
		return FALSE;

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

	inputs = list_prepend (inputs, input);
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
		inputs = list_remove (inputs, input);
		inputs_free = list_prepend (inputs_free, input);

		timer_remove (input->validate);
		input->validate = 0;

		removed++;
	}
}

void input_remove (Connection *c)
{
	input_remove_full (NULL, c, 0, NULL);
}

void input_suspend (Connection *c)
{
	Input *input;

	input = input_find (NULL, c ? c->fd : -1, 0, NULL);
	if(input)
		input->suspended = TRUE;
	else
		TRACE(("AHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH"));
}

void input_resume (Connection *c)
{
	Input *input;

	input = input_find (NULL, c ? c->fd : -1, 0, NULL);
	if(input)
		input->suspended = FALSE;
	else
		TRACE(("NOOOOOOOOOOO            OOOOOOOOOOOOOO"));
}
