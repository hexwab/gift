/*
 * $Id: stopwatch.h,v 1.10 2003/02/09 22:54:33 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#ifndef __STOPWATCH_H
#define __STOPWATCH_H

/*****************************************************************************/

/**
 * @file stopwatch.h
 *
 * @brief Simple portable stopwatch timer implementation
 *
 * Calculates the elapsed time between calls.
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

/**
 * StopWatch structure
 */
typedef struct
{
	unsigned int active : 1;           /**< Current timer state */
	struct timeval start;              /**< Start time */
	struct timeval end;                /**< Stop time */
} StopWatch;

/*****************************************************************************/

/**
 * Allocate a new StopWatch structure.
 *
 * @param start Execute ::stopwatch_start after a successful allocation.
 */
StopWatch *stopwatch_new (int start);

/**
 * Unallocate all memory associated with the StopWatch structure.
 */
void stopwatch_free (StopWatch *sw);

/**
 * Helper identical to calling ::stopwatch_elapsed, and then immediately
 * ::stopwatch_free.  Useful when you want a simple single execution timer
 * instance.
 */
double stopwatch_free_elapsed (StopWatch *sw);

/**
 * Start the timer.
 */
void stopwatch_start (StopWatch *sw);

/**
 * Stop the timer.
 */
void stopwatch_stop (StopWatch *sw);

/**
 * Calculate the amount of time elapsed between start and stop.  If the
 * timer is currently active, the time up until this call is calculated.
 *
 * @param sw
 * @param msec Storage location to put number of microseconds.
 *
 * @retval Total elapsed time in seconds (with millisecond precision).
 */
double stopwatch_elapsed (StopWatch *sw, unsigned long *msec);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __STOPWATCH_H */
