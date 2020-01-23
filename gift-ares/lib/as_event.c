/*
 * $Id: as_event.c,v 1.21 2006/01/21 12:29:53 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"
#include "event.h"    /* libevent */

/*****************************************************************************/

/*
 * This file was originally written under the assumption that libevent
 * supports multiple events per fd which turned out to be not true. Since our
 * ares lib has no need for multiple events per fd this is an internal problem
 * and I have worked around it by removing the event from libevent before its
 * event object is actually removed here. See input_remove for details.
 *
 * For the above reason a rewrite of this file is indicated. Do we also need
 * thread safety?
 */

/*****************************************************************************/

#undef LOG_LIBEVENT_OUTPUT /* also enable USE_DEBUG in libevent */
#undef LOG_A_LOT

/*****************************************************************************/

typedef enum
{
	AS_EVINPUT,
	AS_EVTIMER
} ASEventType;

typedef struct as_event_t
{
	ASEventType type;

	union
	{
		/* timer data */
		struct
		{
			struct timeval interval;
			TimerCallback  cb;
		} timer;

		/* input data */
		struct
		{
			int fd;
			InputState state;
			as_bool suspended;
			struct timeval timeout;
			as_bool validate;
			InputCallback cb;
		} input;
	} d;

	/* user data */
	void *udata;

	/* *sigh*, this is getting ugly */
	as_bool in_callback;
	as_bool in_callback_removed;

	/* Pointer to next event with same fd. Used in fd hash table. */
	struct as_event_t *next;

	/* the libevent struct */
	struct event ev;

} ASEvent;

/*****************************************************************************/

/* We keep a table of all input events in order to look them up by fd */
static ASHashTable *input_table = NULL;

/*****************************************************************************/

#ifdef LOG_LIBEVENT_OUTPUT

static void libevent_log_cb (int severity, const char *msg)
{
	char *level = NULL;
 
	switch (severity)
	{
	case _EVENT_LOG_DEBUG: level = "DBG"; break;
	case _EVENT_LOG_MSG:   level = "MSG"; break;
	case _EVENT_LOG_WARN:  level = "WARN"; break;
	case _EVENT_LOG_ERR:   level = "ERR"; break;
	default:               level = ""; break;
	}

	AS_DBG_2 ("LIBEVENT %s: %s", level, msg);
}

#endif

/* init event system */
as_bool as_event_init ()
{
	assert (input_table == NULL);

	if (!(input_table = as_hashtable_create_int ()))
		return FALSE;
	
	/* initialize libevent */
	event_init ();

	AS_DBG_2 ("Using libevent version %s with %s",
	          event_get_version (), event_get_method ());

#ifdef LOG_LIBEVENT_OUTPUT
	event_set_log_callback (libevent_log_cb);
#endif

	return TRUE;
}

/* shutdown event system. do not call while loop is still running */
void as_event_shutdown ()
{
	/* FIXME: free remaining events? */

	as_hashtable_free (input_table, FALSE);
	input_table = NULL;

#ifdef LOG_LIBEVENT_OUTPUT
	event_set_log_callback (NULL);
#endif

	return;
}

/* event loop, blocks until the loop is quit. returns FALSE if there was an
 * error.
 */
as_bool as_event_loop ()
{
	int ret;

	ret = event_dispatch ();

	if (ret < 0)
	{
		AS_ERR_1 ("Event loop terminated with error. errno = %d", errno);
		return FALSE;
	}

	AS_DBG ("Event loop terminated gracefully.");

	return TRUE;
}

/* stops event loop and makes as_event_loop return */
void as_event_quit ()
{
	event_loopexit (NULL);

	AS_DBG ("Forcing event loop to quit. Better had removed all events for this!");
}

/*****************************************************************************/

static ASEvent *event_create (ASEventType type, void *udata)
{
	ASEvent *ev;
	
	if (!(ev = malloc (sizeof (ASEvent))))
	{
		AS_ERR ("event_create: couldn't alloc event object!");
		return NULL;
	}

	ev->type = type;
	ev->udata = udata;

	ev->d.timer.interval.tv_sec = 0;
	ev->d.timer.interval.tv_usec = 0;
	ev->d.timer.cb = NULL;

	ev->d.input.fd = -1;
	ev->d.input.state = 0;
	ev->d.input.suspended = 0;
	ev->d.input.timeout.tv_sec = 0;
	ev->d.input.timeout.tv_usec = 0;
	ev->d.input.validate = FALSE;
	ev->d.input.cb = NULL;

	ev->in_callback = FALSE;
	ev->in_callback_removed = FALSE;
	
	ev->next = NULL;

	/* zero libevent struct */
	memset (&ev->ev, 0, sizeof (ev->ev));

	return ev;
}

static void event_free (ASEvent *ev)
{
	if (!ev)
		return;

	free (ev);
}

/*****************************************************************************/

/* the function which gets called by libevent for all events */
static void libevent_cb (int fd, short event, void *arg)
{
	ASEvent *ev = (ASEvent *) arg;
	as_bool ret;

#ifdef LOG_A_LOT
	AS_HEAVY_DBG_3 ("libevent_cb: fd: %d, event: 0x%02x, ev: %p", fd,
	                event, arg);
#endif

	if (ev->type == AS_EVTIMER)
	{
		assert (fd == -1);
		assert (event & EV_TIMEOUT);

		ev->in_callback = TRUE;
		ev->in_callback_removed = FALSE;

		/* raise the callback */
		ret = ev->d.timer.cb (ev->udata);

		ev->in_callback = FALSE;

		/* Reset timer if callback returned true. */
		if (ret)
		{
			if (ev->in_callback_removed)
			{
				/* Callback removed timer and now wants to reset it. */
				AS_ERR_1 ("Callback requested reset of removed timer (%p)", ev);
				
				timer_remove (ev);
				assert (0);
			}
			else
			{
				timer_reset (ev);	
			}
		}
		else
		{
			timer_remove (ev);
		}	
	}
	else if (ev->type == AS_EVINPUT)
	{
		assert (fd == ev->d.input.fd);
		assert (fd >= 0);

		if (event & EV_TIMEOUT)
		{
			ev->in_callback = TRUE;
			ev->in_callback_removed = FALSE;

			/* libgift closes fd and removes all inputs. WTF? */
			net_close (ev->d.input.fd);
			input_remove_all (ev->d.input.fd);
			
			/* raise callback with bad fd and no input_id */
			ev->d.input.cb (-1, 0, ev->udata);

			ev->in_callback = FALSE;

			/* event is persistent so remove it now no matter whether callback
			 * requested it or not. */
			input_remove (ev);
		}
		else if (event & (EV_READ | EV_WRITE))
		{
			ev->in_callback = TRUE;
			ev->in_callback_removed = FALSE;
			
			/* raise callback */
			ev->d.input.cb (fd, (input_id)ev, ev->udata);

			ev->in_callback = FALSE;
	
			/* remove input if requested by callback */
			if (ev->in_callback_removed)
			{
				input_remove (ev);
			}
			else if (ev->d.input.validate)
			{
				/* libgift removes the timer after the first succesfull input
				 * event. libevent resets the timeout so we remove the input and
				 * add it again without a timer.
				 */	
				ev->d.input.validate = FALSE;
	
				if (event_del (&ev->ev) != 0)
				{
					AS_ERR ("libevent_cb: event_del() failed!");
					abort (); /* FIXME: handle error properly */
					return;
				}
				
				if (event_add (&ev->ev, NULL) != 0)
				{
					AS_ERR ("libevent_cb: event_add() failed!");
					abort (); /* FIXME: handle error properly */
					return;
				}
			}
		}
		else
		{
			/* cannot happen */
			assert (0);
		}
	}
	else
	{
		/* cannot happen */
		assert (0);
	}
}

/*****************************************************************************/

input_id input_add (int fd, void *udata, InputState state,
                    InputCallback callback, time_t timeout)
{
	ASEvent *ev;
	int ret;
	short trigger;

	assert (callback);
	assert (fd >= 0);

	if (!(ev = event_create (AS_EVINPUT, udata)))
		return INVALID_INPUT;

	ev->d.input.fd = fd;
	ev->d.input.state = state;
	ev->d.input.suspended = FALSE;
	ev->d.input.timeout.tv_sec = timeout / 1000;
	ev->d.input.timeout.tv_usec = (timeout % 1000) * 1000;
	ev->d.input.validate = (ev->d.input.timeout.tv_sec > 0) || 
	                     (ev->d.input.timeout.tv_usec > 0);
	ev->d.input.cb = callback;

	trigger = ((ev->d.input.state & INPUT_READ)  ? EV_READ   : 0) |
	          ((ev->d.input.state & INPUT_WRITE) ? EV_WRITE  : 0) |
	          EV_PERSIST;

	/* Not supported by libevent except for my hacked windows build.
	 * On windows all fds with EV_WRITE will also be added to the exception
	 * set by libevent to work around non-standard behaviour in window's
	 * select().
	 */
	assert ((ev->d.input.state & INPUT_ERROR) == 0);

#ifdef LOG_A_LOT
	AS_HEAVY_DBG_3 ("input_add: fd: %d, event: 0x%02x, ev: %p", fd,
	                trigger, ev);
#endif

	event_set (&ev->ev, ev->d.input.fd, trigger, libevent_cb, (void *)ev);

	if (ev->d.input.validate)
		ret = event_add (&ev->ev, &ev->d.input.timeout);
	else
		ret = event_add (&ev->ev, NULL);
	
	if (ret != 0)
	{
		AS_ERR ("input_add: event_add() failed!");
		event_free (ev);
		return INVALID_INPUT;
	}

	/* Add to hash table. If there already is an entry with this fd keep it. */
	ev->next = as_hashtable_lookup_int (input_table, (as_uint32) fd);
	if (!as_hashtable_insert_int (input_table, (as_uint32) ev->d.input.fd, ev))
	{
		AS_ERR_1 ("Failed to add fd 0x%X into hashtable", ev->d.input.fd);
		assert (0);
	}

	return (input_id) ev;
}

void input_remove (input_id id)
{
	ASEvent *ev = (ASEvent *) id;
	ASEvent *head_ev, *itr_ev;

	if (id == INVALID_INPUT)
		return;

#ifdef LOG_A_LOT
	AS_HEAVY_DBG_2 ("input_remove: fd: %d, ev: %p", ev->d.input.fd, ev);
#endif

	/* Remove event even if we are in a callback since libevent doesn't work
	 * properly with more than one event per fd. The ares lib frequently
	 * removes and readds events for the same fd in one callback. When we are
	 * called again after the callback event_del() will be a noop.
	 */
	if (event_del (&ev->ev) != 0)
		AS_ERR ("input_remove: event_del() failed!");

	if (ev->in_callback)
	{
		/* callback wrapper will remove the input object when it's safe */
		ev->in_callback_removed = TRUE;
		return;
	}

	/* Remove from hash table. */
	if (!(head_ev = as_hashtable_remove_int (input_table, (as_uint32) ev->d.input.fd)))
	{
		AS_ERR_1 ("Failed to remove fd 0x%X from hashtable", ev->d.input.fd);
		assert (0);
	}
	
	/* Reinsert other events with same fd. */
	if (head_ev == ev)
	{
		head_ev = head_ev->next;
	}
	else
	{
		for (itr_ev = head_ev; itr_ev->next; itr_ev = itr_ev->next)
		{
			if (itr_ev->next == ev)
			{
				itr_ev->next = itr_ev->next->next;
				break;
			}
		}
	}

	if (head_ev)
	{
		if (!as_hashtable_insert_int (input_table, (as_uint32) head_ev->d.input.fd, head_ev))
		{
			AS_ERR_1 ("Failed to readd fd 0x%X into hashtable", head_ev->d.input.fd);
			assert (0);
		}	
	}

	event_free (ev);
}

/* remove all inputs of this fd */
void input_remove_all (int fd)
{
	ASEvent *ev, *next_ev;

#ifdef LOG_A_LOT
	AS_HEAVY_DBG_1 ("input_remove_all: fd: %d", fd);
#endif

	if (!(ev = as_hashtable_lookup_int (input_table, (as_uint32) fd)))
	{
#if 0
		AS_WARN_1 ("input_remove_all: Didn't find events for fd 0x%X in hash table",
		           fd);
#endif
		return;
	}

	/* Remove all events with this fd. */
	while (ev)
	{
		next_ev = ev->next;
		input_remove (ev);
		ev = next_ev;
	}
}

/* temporarily remove fd from event loop */
void input_suspend_all (int fd)
{
	assert (0);
}

/* put fd back into event loop */
void input_resume_all (int fd)
{
	assert (0);
}

/*****************************************************************************/

timer_id timer_add (time_t interval, TimerCallback callback, void *udata)
{
	ASEvent *ev;

	assert (callback);

	if (!(ev = event_create (AS_EVTIMER, udata)))
		return INVALID_TIMER;

	ev->d.timer.cb = callback;
	ev->d.timer.interval.tv_sec = interval / 1000;
	ev->d.timer.interval.tv_usec = (interval % 1000) * 1000;

	event_set (&ev->ev, -1, 0, libevent_cb, (void *)ev);
	
	if (event_add (&ev->ev, &ev->d.timer.interval) != 0)
	{
		AS_ERR ("timer_add: event_add() failed!");
		event_free (ev);
		return INVALID_TIMER;
	}

	return (timer_id) ev;
}

void timer_reset (timer_id id)
{
	ASEvent *ev = (ASEvent *) id;

	if (id == INVALID_TIMER)
		return;

	if (ev->in_callback && ev->in_callback_removed)
	{
		AS_ERR_1 ("Tried to reset a removed timer (%p).", id);
		assert (0);
		return;
	}

	/* simply add it again, this reset the timeout if it was already added */
	if (event_add (&ev->ev, &ev->d.timer.interval) != 0)
	{
		AS_ERR ("timer_reset: event_add() failed!");
		event_free (ev);
		return;
	}
}

void timer_remove (timer_id id)
{
	ASEvent *ev = (ASEvent *) id;

	if (id == INVALID_TIMER)
		return;

	if (ev->in_callback)
	{
		/* callback wrapper will remove the timer when it's save */
		ev->in_callback_removed = TRUE;
		return;
	}

	if (event_del (&ev->ev) != 0)
		AS_ERR ("timer_remove: event_del() failed!");

	event_free (ev);
}

void timer_remove_zero (timer_id *id)
{
	assert (id);

	timer_remove (*id);
	*id = INVALID_TIMER;
}

/*****************************************************************************/

