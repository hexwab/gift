/*
 * event.h
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

#ifndef __EVENT_H
#define __EVENT_H

/*****************************************************************************/

/**
 * @file event.h
 *
 * @brief Main select event loop.
 *
 * Provides the low-level framework for giFT file descriptor (yes, that
 * includes sockets) handling.
 */

/*****************************************************************************/

#include <sys/types.h>

#include "protocol.h"
#include "connection.h"

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#define MSEC     (1)
#define SECONDS  (1000*MSEC)
#define MINUTES  (60*SECONDS)

/*****************************************************************************/

/**
 * Callback used to indicate that the connection has changed state according
 * to the ::input_add state.
 */
typedef void (*InputCallback) (Protocol *p, Connection *c);

/**
 * Available select sets.
 */
typedef enum
{
	INPUT_READ      = 0x01,
	INPUT_WRITE     = 0x02,
	INPUT_EXCEPTION = 0x04
} InputState;

/**
 * File descriptor handler.
 */
typedef struct
{
	int           fd;                  /**< file descriptor */
	InputState    state;

	InputCallback callback;
	Protocol     *protocol;            /**< protocol which requested this
										*   handler.  used for cleanup. */
	Connection   *data;                /**< connection that controls this fd */

	int           complete;            /**< complete is FALSE until first
										*   state change.  used for
										*   automated connection timeouts. */
	unsigned long validate;            /**< timeout for incomplete sockets */
	int           suspended;           /**< suspended is TRUE when input
										*   should not be put in select loop */
} Input;

/*****************************************************************************/

/**
 * Timer has reached zero.
 *
 * @param data Arbitrary user data.
 *
 * @retval FALSE Clear and remove from event loop.
 * @retval TRUE  Reset this timer.
 */
typedef int (*TimerCallback) (void *data);

/**
 * Timer event.
 */
struct _timer
{
	unsigned long  id;                 /**< reference identifier */

	struct timeval expiration;         /**< exact time this timer will
										*   expire */
	struct timeval interval;           /**< interval this timer is set for */

	TimerCallback  callback;
	void          *data;               /**< arbitrary user data */
};

#ifndef IMAGE_MAGICK_SUCKS
typedef struct _timer Timer;
#endif /* IMAGE_MAGICK_SUCKS */

/*****************************************************************************/

/**
 * Clear all events and return from the main loop.  This will not exit your
 * program, merely ::event_loop.
 *
 * @param dummy No idea why I did this.
 */
void event_quit (int dummy);

/**
 * Begin the main event loop.  If you have not registered any events, your
 * program will effectively do nothing.
 */
void event_loop ();

/*****************************************************************************/

/**
 * Install a new timer event.
 *
 * @param interval Time to wait.  Expressed in milliseconds.  See ::SECONDS
 *                 and ::MINUTES for helpers.
 * @param callback
 * @param data     Arbitrary user data.
 *
 * @return Unique timer identifier on success, otherwise zero.
 */
unsigned long timer_add (time_t interval, TimerCallback callback, void *data);

/**
 * Reset a timer.  That is, reset the expiration time to now + interval.
 *
 * @param id
 */
void timer_reset (unsigned long id);

/**
 * Remove a timer.  This clears all data associated with the timer.
 * The callback will not be raised.  There is no way of determining success
 * or failure.
 */
void timer_remove (unsigned long id);

/*****************************************************************************/

/**
 * Add an fd handler.
 *
 * @param p        Protocol which registered this input.
 * @param c
 * @param state    Select set.
 * @param callback
 * @param timeout  If non-zero, initiates a timeout which will be activated
 *                 if no state changes occur on this socket in a reasonable
 *                 amount of time.
 */
void input_add (Protocol *p, Connection *c, InputState state,
                InputCallback callback, int timeout);

/**
 * Remove the input matching the supplied parameters.  Avoid using this
 * function, instead try ::input_remove.
 *
 * @param p
 * @param c
 * @param state
 * @param cb
 */
void input_remove_full (Protocol *p, Connection *c, InputState state,
                        InputCallback cb);

/**
 * Remove all inputs registered to the supplied connection.
 */
void input_remove (Connection *c);

/**
 * Suspend all inputs registered to this connection from the select
 * loop.  This is useful for bandwidth throttling in user space and some
 * other evil things.
 */
void input_suspend (Connection *c);

/**
 * Check if the given connection has suspended inputs.
 */
int input_suspended (Connection *c);

/**
 * Move the inputs back into the select loop.  See ::input_suspend.
 */
void input_resume (Connection *c);

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __EVENT_H */
