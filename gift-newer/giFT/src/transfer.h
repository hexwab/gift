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

/*****************************************************************************/

/* runtime switches determine whether or not this code is actually used */
#define THROTTLE_ENABLE

#ifdef THROTTLE_ENABLE
#define TICKS_PER_SEC     10    /* the default rate of throttler */
#define TICK_INTERVAL     (((1000 * MSEC) / TICKS_PER_SEC)*MSEC)
#define THROTTLE_TIME     (3*SECONDS)
#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

typedef enum _transfer_type
{
	TRANSFER_DOWNLOAD,
	TRANSFER_UPLOAD
} TransferType;

typedef struct _transfer
{
	IFEventID     id;

	TransferType  type;         /* the type of the transfer */
	char         *filename;     /* the name of the file */
	char         *path;         /* fully qualified local path to this file */

	char         *hash;         /* protocol-specific hash to reference this
								 * transfer */

	int           active;       /* inactive when first created with 0 sources */

	unsigned long transmit;     /* total received */
	unsigned long transmit_old; /* previous total received */
	unsigned long throughput;   /* bytes/second throughput */
	unsigned long total;        /* total filesize to download */
	unsigned long max_seek;     /* current filesize according to the filesystem */

	List         *chunks;       /* partitioned download segments */
	List         *sources;      /* sources used for downloading */

	unsigned long timer;        /* logs the download progress in the resume file */

	FILE         *f;            /* read/write file descriptor */

	/* DOWNLOAD SPECIFIC */

	char         *state_path;

	/* UPLOAD SPECIFIC */

	int           display;      /* temporarily hide this transfer from the
								 * interface protocol (until its been completely
								 * filled) */

} Transfer;

/*****************************************************************************/

typedef enum
{
	SOURCE_UNUSED = 0,       /* this source is unused and (possibly) waiting
	                          * to be placed */
	SOURCE_QUEUED_REMOTE,    /* protocol says the other end is preventing us
	                          * from downloading */
	SOURCE_QUEUED_LOCAL,     /* we are preventing ourselves from downloading
	                          * this file */
	SOURCE_WAITING,          /* asked the protocol to download but haven't
	                          * received any status back */
	SOURCE_ACTIVE,           /* set once data has started coming in */
} SourceStatus;

typedef struct _source
{
	Protocol      *p;        /* p->name = OpenFT */

	struct _chunk *chunk;    /* chunk using this source */

	SourceStatus   status;

	char          *user;     /* 65.4.102.175                           */
	char          *hash;     /* 3da9f138de951c22c8a850899680a9e4       */
	char          *url;      /* OpenFT://65.4.102.175:1216/file.tar.gz */
} Source;

/*****************************************************************************/

typedef struct _chunk
{
	Transfer     *transfer;      /* parent */

	Source       *source;        /* source fulfilling this chunk's range */

	float         adjust;        /* how much to adjust the rw buffer */
	unsigned long transmit;      /* currently received */
	unsigned long transmit_old;
	unsigned long throughput;
	unsigned long tmp_recv;      /* k/s calculations, timeout */
	unsigned long start;
	unsigned long stop;

	int           tick_cap;      /* number of ticks this chunk can be
								  * active in select loop */

	int           timeout_cnt;   /* timeout counter (how many seconds of
								  * inactivity exist) */
	int           timeout_max;   /* maximum number of timeout counts until an
								  * action is performed */

	void         *data;          /* protocol-specific data */
} Chunk;

/*****************************************************************************/

Transfer  *transfer_new            (TransferType direction, char *filename,
									char *hash, unsigned long size);
void       transfer_free           (Transfer *transfer);
void       transfer_stop           (Transfer *transfer, int cancel);
#ifdef THROTTLE_ENABLE
int        transfer_calc_bw        (Transfer *transfer);
#endif
int        transfers_throttle_tick (List *transfers, unsigned short *tick_count);
int        transfers_throttle      (List *transfers, unsigned long max_bw);

/*****************************************************************************/

Source    *source_new              (char *user, char *hash, char *url);
void       source_free             (Source *source);
int        source_equal            (Source *a, Source *b);
char      *source_status           (Source *source);

/*****************************************************************************/

Chunk     *chunk_new               (Transfer *transfer, Source *source,
							        size_t start, size_t stop);
void       chunk_write             (Chunk *chunk, char *segment, size_t len);
void       chunk_free              (Chunk *chunk);
void       chunk_cancel            (Chunk *chunk);
#ifdef THROTTLE_ENABLE
int        chunk_calc_bw           (Chunk *transfer);
#endif

/*****************************************************************************/

#endif /* __TRANSFER_H */
