/*
 * Copyright (C) 2003 Arend van Beelen jr. (arend@auton.nl)
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

#ifndef __SL_STATS_H
#define __SL_STATS_H

#include "sl_soulseek.h"

/*****************************************************************************/

typedef struct
{
	char *name;
	unsigned int users;
} SLRoom;

typedef struct
{
	unsigned int users;
	unsigned int files;
	unsigned int size;  // GB
	unsigned int numrooms;
	SLRoom *rooms;
} SLStats;

/*****************************************************************************/

// initialize stats
SLStats *sl_stats_create();

// destroy stats
void sl_stats_destroy(SLStats *stats);

// giFT callback to retrieve stats
int sl_gift_cb_stats(Protocol *p, unsigned long *users, unsigned long *files, double *size, Dataset **extra);

// receives a new roomlist
void sl_stats_receive_roomlist(SLStats *stats, uint8_t *data);

// sends a request for the global userlist
void sl_stats_send_global_userlist_request(SLSession *session);

// receives the global userlist
void sl_stats_receive_global_userlist(SLStats *stats, uint8_t *data);

/*****************************************************************************/

#endif /* __SL_STATS_H */
