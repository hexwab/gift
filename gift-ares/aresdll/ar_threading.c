/*
 * $Id: ar_threading.c,v 1.1 2005/12/18 16:43:38 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include <process.h> /* _beginthreadex */

#include "aresdll.h"
#include "ar_callback.h"
#include "as_ares.h"

/*****************************************************************************/

static unsigned long ar_thread_handle = 0; /* handle to event system thread */
static int event_fd = -1;   /* end of socket pair used by event thread */
static int outside_fd = -1; /* end of socket pair used by outside threads */

/*****************************************************************************/

static void interrupt_cb (int fd, input_id id, void *udata)
{
	int spin = 0;
	int i;

#if 0
	AS_HEAVY_DBG ("INTR (event thread)");
#endif

	/* wait until spin is back to zero */
	do {
		/* blocks */
		if (recv (fd, (char *)&i, sizeof (int), 0) == sizeof (int))
		{
			spin += i;

			/* send back current spin count */
			if (send (event_fd, (char *)&spin, sizeof (int), 0) != sizeof (int))
				AS_ERR ("Failed to send spin count");
		}
	} while (spin > 0);

#if 0
	AS_HEAVY_DBG ("RESM (event thread)");
#endif
}

/* the thread which runs our event system */
static unsigned int __stdcall ar_thread_func (void *data)
{
	int event_fd = (int) data;
	input_id interrupt_id;
	int i;

	/* init event system */
	if (!as_event_init ())
	{
		AS_ERR ("Failed to start event system");
		return 1;
	}

	/* add our interrupt fd */
	interrupt_id = input_add (event_fd, NULL, INPUT_READ, interrupt_cb, 0);

	if (interrupt_id == INVALID_INPUT)
	{
		AS_ERR ("Failed to add interrupt fd to event system");
		return 1;
	}

	/* signal creating thread that we are ready */
	i = 0;
	if (send (event_fd, (char *)&i, sizeof (int), 0) != sizeof (int))
	{
		AS_ERR ("Failed to send ready signal to creating thread");
		return 1;
	}

	/* run event loop */
	AS_DBG ("Entering event loop");
	as_event_loop ();
	AS_DBG ("Left event loop");

	/* remove fd */
	input_remove (interrupt_id);

	/* shutdown event system */
	as_event_shutdown ();

	AS_DBG ("Exiting event thread");

	/* signal creating thread that we are finished */
	i = 0;
	if (send (event_fd, (char *)&i, sizeof (int), 0) != sizeof (int))
	{
		AS_ERR ("Failed to send complete signal to creating thread");
		return 1;
	}

	return 0;
}

/*****************************************************************************/

/*
 * Pause event system from different thread so ares lib can be accessed
 * safely.
 */
as_bool ar_events_pause ()
{
	int i = 1;

	if (ar_thread_handle == 0)
		return FALSE;

	/* if we are in a callback the even thread is already blocked */
	if (ar_callback_active ())
		return TRUE;

	/* send interrupt signal */
	if (send (outside_fd, (char *)&i, sizeof (int), 0) != sizeof (int))
	{
		AS_ERR ("ar_events_pause: Failed to send interrupt signal.");
		assert (0);
		return FALSE;
	}

	/* wait until the thread has interrupted */
	if (recv (outside_fd, (char *)&i, sizeof (int), 0) < sizeof (int))
	{
		AS_ERR ("Didn't receive ready signal from event thread.");
		assert (0);
		return FALSE;
	}
	assert (i > 0);

	AS_HEAVY_DBG_1 ("INTR [%d]", i);

	return TRUE;
}

/*
 * Resume pause event system.
 */
as_bool ar_events_resume ()
{
	int i = -1;

	if (ar_thread_handle == 0)
		return FALSE;

	/* if we are in a callback the even thread will be resumed later */
	if (ar_callback_active ())
		return TRUE;

	/* send interrupt signal */
	if (send (outside_fd, (char *)&i, sizeof (int), 0) != sizeof (int))
	{
		AS_ERR ("ar_events_pause: Failed to send resume signal.");
		assert (0);
		return FALSE;
	}

	/* wait until the thread has decreased spin count */
	if (recv (outside_fd, (char *)&i, sizeof (int), 0) < sizeof (int))
	{
		AS_ERR ("Didn't receive ready signal from event thread.");
		assert (0);
		return FALSE;
	}

	/* Note: if i is still > 0 we don't resume just yet */

	AS_HEAVY_DBG_1 ("RESM [%d]", i);

	return TRUE;
}

/*****************************************************************************/

as_bool ar_start_event_thread ()
{
	int fds[2];
	BOOL nagle;
	int i;

	if (ar_thread_handle != 0)
	{
		/* logger already active at this point */
		AS_ERR ("Tried to startup ares dll twice.");
		return FALSE;
	}

	assert (event_fd == -1);
	assert (outside_fd == -1);

	AS_DBG ("Starting event thread");

	/* create socketpair to interrupt event thread from outside */
	if (socketpair (0, 0, 0, fds) < 0)
	{
		AS_ERR ("Couldn't create socket pair.");
		return FALSE;
	}

	outside_fd = fds[0];
	event_fd = fds[1];

	/* disable nagle for the socket pair */
	nagle = TRUE;
	if (setsockopt (outside_fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&nagle,
	                sizeof (nagle)) != 0)
	{
		AS_WARN ("Couldn't disable nagle algo for outside fd");
	}
	nagle = TRUE;
	if (setsockopt (event_fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&nagle,
	                sizeof (nagle)) != 0)
	{
		AS_WARN ("Couldn't disable nagle algo for event fd");
	}

	/* create event system thread */
	if ((ar_thread_handle = _beginthreadex (NULL, 0, ar_thread_func,
	                                        (void *)event_fd, 0, NULL)) == 0)
	{
		AS_ERR ("Couldn't start event system thread.");
		closesocket (outside_fd);
		closesocket (event_fd);
		outside_fd = -1;
		event_fd = -1;
		return FALSE;
	}

	/* wait until the event thread is ready */
	if (recv (outside_fd, (char *)&i, sizeof (int), 0) != sizeof (int))
	{
		AS_ERR ("Didn't receive ready signal from event thread.");
		closesocket (outside_fd);
		closesocket (event_fd);
		outside_fd = -1;
		event_fd = -1;
		return FALSE;
	}
	assert (i == 0);

	AS_DBG ("Event thread started succesfully.");

	return TRUE;
}

as_bool ar_stop_event_thread ()
{
	int i;

	ar_events_pause ();

	AS_DBG ("Stoppting event thread");

	/* quit event loop */
	as_event_quit ();

	ar_events_resume ();

	/* wait for completion of event thread */
	if (recv (outside_fd, (char *)&i, sizeof (int), 0) != sizeof (int))
		AS_ERR ("Didn't receive complete signal from event thread.");
	else
		assert (i == 0);

	AS_DBG ("Event thread stopped succesfully.");

	/* close socket pair */
	closesocket (outside_fd);
	closesocket (event_fd);
	outside_fd = -1;
	event_fd = -1;

	/* reset thread handle */
	ar_thread_handle = 0;

	return TRUE;
}

/*****************************************************************************/
