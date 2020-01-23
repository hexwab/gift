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

#include <time.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>

#include "gift.h"

#include "network.h"
#include "event.h"
#include "protocol.h"
#include "connection.h"

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
                            time_t *lowest_time)
{
	if (*lowest_time == 0 || timer->expiration < *lowest_time)
		*lowest_time = timer->expiration;

	return TRUE;
}

static time_t next_timer (time_t current_time)
{
	time_t lowest_time = 0;

	hash_table_foreach (timers, (HashFunc) calc_next_timer, &lowest_time);

	if (lowest_time < current_time)
		return 0;

	return (lowest_time - current_time);
}

static void dispatch_timers (time_t current_time)
{
	List *safe_copy, *ptr;

	safe_copy = hash_flatten (timers);

	for (ptr = safe_copy; ptr; ptr = ptr->next)
	{
		Timer *timer = ptr->data;
		int ret;

		if (timer->expiration > current_time)
			continue;

		ret = (*timer->callback) (timer->data);

		if (!ret)
			timer_remove (timer->id);
		else
			timer->expiration = current_time + timer->interval;
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
	safe_copy = list_copy (inputs);

	for (ptr = safe_copy; ptr; ptr = ptr->next)
	{
		Input *input     = ptr->data;
		int    safe      = TRUE;
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
	time_t current_time;

	while (loop_running)
	{
		current_time = time (NULL);

		/* TODO - next_timer should probably have microsecond precision */
		next_event.tv_sec = next_timer (current_time);
		next_event.tv_usec = 0;
		maxfd = prepare_fds (&rset, &wset, &xset);

		if (select (maxfd + 1, &rset, &wset, &xset,
		            timers ? &next_event : NULL) < 0)
		{
			/* oh shit */
			perror ("select");
#if 0
			dump_events ();
			abort ();
#endif
		}

		/* dispatch */
		dispatch_inputs (&rset, &wset, &xset);
		dispatch_timers (current_time);

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

int timer_add (time_t interval, TimerCallback callback, void *data)
{
	Timer *timer;
	time_t current_time;
	unsigned long uniq_id;

	if (!(uniq_id = timer_uniq_id ()))
		return uniq_id;

	timer = malloc (sizeof (Timer));

	timer->id         = uniq_id;

	timer->interval   = interval;

	current_time      = time (NULL);
	timer->expiration = current_time + interval;

	timer->callback   = callback;
	timer->data       = data;

	if (!timers)
		timers = hash_table_new ();

	hash_table_insert (timers, uniq_id, timer);

	return uniq_id;
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

static Input *input_find (int fd, InputState state, InputCallback cb)
{
	List *ptr;

	for (ptr = inputs; ptr; ptr = ptr->next)
	{
		Input *input = ptr->data;

		if (cb && input->callback != cb)
			continue;

		if (state && input->state != state)
			continue;

		if (input->fd == fd)
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

	/* notify the input holder that it timed out ... it will be passed an invalid socket (via input->data),
	 * hopefully it can handle that */
	(*input->callback) (input->protocol, input->data);

	/* input->callback may have removed it from the select loop, but we really
	 * can't be sure of that...so check here */
	input_remove (input->data);

	return FALSE;
}

void input_add (Protocol *p, Connection *c, InputState state,
                InputCallback callback, int timeout)
{
	Input *input;

	if (!c || c->fd < 0)
		return;

	input = malloc (sizeof (Input));

	input->fd       = c->fd;
	input->state    = state;
	input->callback = callback;
	input->protocol = p;        /* tracking */
	input->data     = c;

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
			timer_add (60, (TimerCallback) validate_timeout, input);
	}

	inputs = list_prepend (inputs, input);
}

void input_remove_full (Connection *c, InputState state, InputCallback cb)
{
	Input *input;
	int removed = 0;

	if (!c || c->fd < 0)
		return;

	while ((input = input_find (c->fd, state, cb)))
	{
		inputs = list_remove (inputs, input);
		inputs_free = list_prepend (inputs_free, input);

		timer_remove (input->validate);

		removed++;
	}
}

void input_remove (Connection *c)
{
	input_remove_full (c, 0, NULL);
}
