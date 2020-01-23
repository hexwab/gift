/*
 * $Id: transfer.h,v 1.49 2003/05/02 10:54:46 jasta Exp $
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

#ifndef __TRANSFER_H
#define __TRANSFER_H

/*****************************************************************************/

#include "protocol.h"
#include "if_event.h"
#include "stopwatch.h"

/*****************************************************************************/

#ifdef DEBUG
/* #define TRANSFER_LOG */
#endif

#include "parse.h"

#define INCOMING_PATH(args) \
	transfer_output_path ("download/incoming", "incoming", stringf args)

#define COMPLETED_PATH(args) \
	transfer_output_path ("download/completed", "completed", stringf args)

/* runtime switches determine whether or not this code is actually used */
#define THROTTLE_ENABLE

#ifdef THROTTLE_ENABLE
# define THROTTLE_TIME     (1*SECONDS)
#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

typedef enum _transfer_type
{
	TRANSFER_DOWNLOAD,
	TRANSFER_UPLOAD
} TransferType;

typedef struct _transfer
{
	IFEvent      *event;

	TransferType  type;                /* the type of the transfer */
	char         *filename;            /* the name of the save file */
	char         *path;                /* fully qualified local path to this
										* file */

#ifdef TRANSFER_LOG
	char         *log_path;            /* activity file */
#endif

	char         *hash;                /* protocol-specific hash to reference
										* this transfer */
	int           active;              /* inactive when first created with 0
										* sources */

	off_t         transmit;            /* total received */
	off_t         transmit_old;        /* previous total received */
	off_t         transmit_change;     /* same as old but for different purposes
										* NOTE: this is temporary */
	unsigned long throughput;          /* bytes/second throughput */
	off_t         total;               /* total filesize to download */
	off_t         max_seek;            /* current filesize according to the
										* filesystem */

	StopWatch    *sw;                  /* throughput calculations */

	int           flag;                /* arbitrary state flag...used by
										* downloads for save_state */

	char         *state;               /* last reported transfer state (sum of
										* all chunks basically) */

	List         *chunks;              /* partitioned download segments */
	List         *sources;             /* sources used for downloading */

	FILE         *f;                   /* read/write file descriptor */

	/* DOWNLOAD SPECIFIC */

	char         *uniq;                /* unique identifer, see uniq_file in
										* download.c */
	char         *state_path;
	unsigned char paused : 1;          /* see SOURCE_PAUSED */
	unsigned char verifying : 1;       /* bla bla, temporary */

	/* UPLOAD SPECIFIC */

	/* temporarily hide this transfer from the interface protocol (until its
	 * been completely filled)
	 * NOTE: this is no longer in use */
	unsigned char display : 1;

	/* special upload (eg authorized by protocol, not giFT) */
	unsigned char shared : 1;
} Transfer;

/*****************************************************************************/

/*
 * TODO: We need a SOURCE_DEAD
 */
typedef enum _source_status
{
	SOURCE_UNUSED = 0,       /* this source is unused and (possibly) waiting
	                          * to be placed */
	SOURCE_PAUSED,           /* source has been explicitly paused by user */
	SOURCE_QUEUED_REMOTE,    /* protocol says the other end is preventing us
	                          * from downloading */
	SOURCE_QUEUED_LOCAL,     /* we are preventing ourselves from downloading
	                          * this file */
	SOURCE_COMPLETE,         /* last known event was that the chunk associated
							  * complete successfully, and is now moving onto
							  * another */
	SOURCE_CANCELLED,        /* remote end cancelled an active transfer */
	SOURCE_TIMEOUT,          /* date timeout */
	SOURCE_WAITING,          /* asked the protocol to download but haven't
	                          * received any status back */
	SOURCE_ACTIVE,           /* set once data has started coming in */
} SourceStatus;

typedef struct _source
{
	Protocol      *p;        /* p->name = OpenFT */

	struct _chunk *chunk;    /* chunk using this source */

	SourceStatus   status;   /* reflects the "last" known status change of this
							  * source, _not_ the current activity.  this is
							  * done because it is far more useful to the
							  * gui developer to see 'completed' then 'active',
							  * rather than active -> waiting -> active. */
	char          *status_data;

	char          *user;     /* 65.4.102.175                           */
	char          *hash;     /* 3da9f138de951c22c8a850899680a9e4       */
	char          *url;      /* OpenFT://65.4.102.175:1216/file.tar.gz */
} Source;

/*****************************************************************************/

typedef struct _chunk
{
	Transfer     *transfer;            /* parent */

	Source       *source;              /* source fulfilling this chunk's
	                                    * range */

	float         adjust;              /* how much to adjust the rw buffer */
	off_t         transmit;            /* currently received */
	off_t         transmit_old;
	unsigned long throughput;
	unsigned long tmp_recv;            /* k/s calculations, timeout */
	off_t         start;
	off_t         stop;

	unsigned char stop_change : 1;     /* stop was moved...protocol might
	                                    * need to know this */
	unsigned char suspended   : 1;     /* chunk was suspended */

	int           timeout_cnt;         /* timeout counter (how many seconds of
	                                    * inactivity exist) */
	int           timeout_max;         /* maximum number of timeout counts
	                                    * until an action is performed */

	void         *data;                /* protocol-specific data */
} Chunk;

/*****************************************************************************/

Transfer  *transfer_new             (TransferType direction, char *filename,
                                     char *hash, off_t size);
void       transfer_free            (Transfer *transfer);
void       transfer_pause           (Transfer *transfer);
void       transfer_unpause         (Transfer *transfer);
void       transfer_stop            (Transfer *transfer, int cancel);

int          transfer_user_cmp      (Source *source, char *user);
unsigned int transfer_length        (List *tlist, char *user, int active);

char      *transfer_output_path     (char *key, char *name, char *filename);
void       transfer_log             (Transfer *transfer, char *fmt, ...);

#ifdef THROTTLE_ENABLE
int        transfer_calc_bw         (Transfer *transfer);
int        transfers_resume         (List *transfers, unsigned long max_bw,
                                     unsigned long *r_credits);
#endif

/*****************************************************************************/

Source    *source_new               (char *user, char *hash, char *url);
void       source_free              (Source *source);
int        source_cmp               (Source *a, Source *b);
char      *source_status            (Source *source, int proto);
void       source_status_set        (Source *source, SourceStatus status,
                                     char *text);

/*****************************************************************************/

Chunk     *chunk_new                (Transfer *transfer, Source *source,
                                     off_t start, off_t stop);
void       chunk_write              (Chunk *chunk, unsigned char *segment,
                                     size_t len);
void       chunk_free               (Chunk *chunk);
void       chunk_cancel             (Chunk *chunk);
#ifdef THROTTLE_ENABLE
int        chunk_calc_bw            (Chunk *transfer);
#endif
size_t     chunk_throttle           (Chunk *chunk, unsigned long max_bw,
                                     size_t len, unsigned long credits);
/*****************************************************************************/

#endif /* __TRANSFER_H */
