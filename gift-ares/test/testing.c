/*
 * $Id: testing.c,v 1.3 2004/08/27 10:25:09 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "testing.h"

/*****************************************************************************/

void input_cb (int fd, input_id input, TCPC *c)
{
	int test_id = (int) c->udata;

	input_remove (input);

	if (fd < 0)
	{
		/* timeout */
		printf ("[%d]   Connect timed out.\n", test_id);
	}
	else if (net_sock_error (fd) != 0)
	{
		/* connect failed */
		printf ("[%d]   Connect failed.\n", test_id);
	}
	else
	{
		/* connected successfully */
		printf ("[%d]   Connection established.\n", test_id);
	}

	printf ("[%d]   Connection test successful. Closing Connection.\n", test_id);
	tcp_close (c);

	printf ("[%d] Event system test complete.\n", test_id);
}


static as_bool timer_cb (int test_id)
{
	static int count = 1;
	TCPC *c;

	printf ("[%d]   Timer callback %d\n", test_id, count);

	if (count < 5)
	{
		count++;
		return TRUE; /* call us again */
	}

	/* reset count for next test */
	count = 1;
	printf ("[%d]   Timer test completed successfully.\n", test_id);

	/* do a socket event test */
	printf ("[%d] Step 2: Trying TCP connection to 127.0.0.1:1111 with 10 "
	        "second timeout...\n", test_id);

	if (!(c = tcp_open (net_ip ("127.0.0.1"), 1111, FALSE)))
	{
		printf ("[%d]   tcp_open() failed.\n", test_id);
		return FALSE;
	}

	c->udata = (void *)test_id;

	if (input_add (c->fd, (void *)c, INPUT_WRITE,
                   (InputCallback)input_cb, 10 * SECONDS) == 0)
	{
		printf ("[%d]   input_add() failed.\n", test_id);
		tcp_close (c);
		return FALSE;
	}

	return FALSE; /* discard this timer */
}

as_bool test_event_system ()
{
	static int test_id = 0;

	printf ("[%d] Testing event system...\n", test_id);
	printf ("[%d] Step 1: Scheduling 5 iterations of a one second timer...\n",
		    test_id);
	
	if (timer_add (1 * SECONDS, (TimerCallback)timer_cb, (void*)test_id) == 0)
	{
		printf ("[%d]   ERROR: input_timer_add() failed\n", test_id);
		return FALSE;
	}

	test_id++;
	return TRUE;
}

/*****************************************************************************/

