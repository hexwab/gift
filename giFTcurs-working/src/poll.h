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
 * $Id: poll.h,v 1.17 2002/09/20 22:32:58 chnix Exp $
 */
#ifndef _POLL_H
#define _POLL_H

typedef unsigned int tick_t;

/* Defines the granulatity of the timer. Number of "ticks" per second.
 * A value of 1000 can cause overflow on 32-bit machines within a month.
 */
#define SECS(x) ((tick_t) (100*(x)))

/* Returns number of ticks since program start */
tick_t uptime(void);

/* A TFunc wants to be called after 'delay' ticks. If it wants to be
 * called again, it returns the new delay in ticks. Otherwise it returns 0.
 */
typedef tick_t(*TFunc) (void *);
int poll_add_timer(tick_t delay, TFunc, void *data);
int poll_del_timer(TFunc);

/* register one file descriptor to watch for incoming data */
typedef void (*FFunc) (int);
int poll_add_fd(int fd, FFunc);
int poll_del_fd(int fd);

/* (de)initializes timer/polling unit */
void timer_init(void);
void timer_destroy(void);

/* start the main poll loop */
void poll_forever(void);

extern void (*ugly_hack) (int);

#endif
