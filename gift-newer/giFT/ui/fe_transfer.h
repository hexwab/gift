/*
 * fe_transfer.h
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

#ifndef __FE_TRANSFER_H
#define __FE_TRANSFER_H

/**************************************************************************/

#include "fe_share.h"
#include "fe_obj.h"

/**************************************************************************/

/* buckets of samples to average upload/download speed over */
#define TR_SAMPLES 10

typedef struct {
	unsigned long id;

	ObjectData obj_data;

	SharedFile shr;         /* shared data ... */

	int xfer;               /* upload/download */
	int active;             /* pending state   */

	unsigned long transmit; /* total data sent/received */
	unsigned long total;    /* total size */
	GList *transfer_rate; /* transfer rate ac */
} Transfer;

#define Transfer_Type 1

/**************************************************************************/

enum {
	TRANSFER_WAITING = 0,
	TRANSFER_ACTIVE,
	TRANSFER_PAUSED,
	TRANSFER_FINISHED,
	TRANSFER_CANCELLED
};

/**************************************************************************/

int transfer_get_pixmap	(Transfer* transfer, GdkPixmap **pixmap,
						 GdkBitmap **bitmap);
void fe_transfer_free		(Transfer *obj);

unsigned long transfer_get_rate (Transfer *tr);
/**************************************************************************/

#endif /* __FE_TRANSFER_H */
