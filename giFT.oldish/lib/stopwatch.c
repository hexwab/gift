/*
 * stopwatch.c
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

#include "stopwatch.h"

#ifdef WIN32
#include <limits.h>
#endif

/*****************************************************************************/

StopWatch *stopwatch_new (int start)
{
	StopWatch *sw;

	if (!(sw = malloc (sizeof (StopWatch))))
		return NULL;

	memset (sw, 0, sizeof (StopWatch));

	if (start)
		stopwatch_start (sw);

	return sw;
}

void stopwatch_free (StopWatch *sw)
{
	free (sw);
}

/*****************************************************************************/

void stopwatch_start (StopWatch *sw)
{
	if (!sw)
		return;

	sw->active = TRUE;

	platform_gettimeofday (&sw->start, NULL);
}

void stopwatch_stop (StopWatch *sw)
{
	if (!sw || !sw->active)
		return;

	sw->active = FALSE;

	platform_gettimeofday (&sw->end, NULL);
}

double stopwatch_elapsed (StopWatch *sw, unsigned long *msec)
{
	double total;
	struct timeval elapsed;

	if (!sw)
		return 0.0;

	/* get the current time but dont actually stop the timer */
	if (sw->active)
	{
		stopwatch_stop (sw);
		sw->active = TRUE;
	}

	if (sw->start.tv_usec > sw->end.tv_usec)
	{
		sw->end.tv_usec += 1000000;
		sw->end.tv_sec--;
	}

	elapsed.tv_usec = sw->end.tv_usec - sw->start.tv_usec;
	elapsed.tv_sec = sw->end.tv_sec - sw->start.tv_sec;

	total = elapsed.tv_sec + ((double) elapsed.tv_usec / 1e6);
	if (total < 0)
	{
		total = 0.0;
		elapsed.tv_usec = 0;
	}

	if (msec)
		*msec = elapsed.tv_usec;

	return total;
}
