/*
 * transfer.h
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

#ifndef __TRANSFER_H
#define __TRANSFER_H

/**************************************************************************/

#include "share.h"
#include "obj.h"

/**************************************************************************/

typedef struct {
	unsigned long id;

	ObjectData obj_data;

	SharedFile shr;         /* shared data ... */

	int xfer;               /* upload/download */
	int active;             /* pending state   */
	int finished;           /* completed state */

	unsigned long transmit; /* total data sent/received */
	unsigned long total;    /* total size */
} Transfer;

#define Transfer_Type 1

/**************************************************************************/

enum {
	TRANSFER_WAITING = 0,
	TRANSFER_ACTIVE,
	TRANSFER_PAUSED,
	TRANSFER_FINISHED
};

/**************************************************************************/

void transfer_free (Transfer *obj);

/**************************************************************************/

#endif /* __TRANSFER_H */
