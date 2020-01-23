/*
 * $Id: asp_hashmap.h,v 1.2 2004/12/19 00:50:14 mkern Exp $
 *
 * Copyright (C) 2004 giFT-Ares project
 * http://developer.berlios.de/projects/gift-ares
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

/*****************************************************************************/

#ifndef __ASP_HASHMAP_H
#define __ASP_HASHMAP_H

#ifdef EVIL_HASHMAP

#include <libgift/stopwatch.h>

/* We have to include this because giFT, umm, doesn't. */
struct transfer
{
	IFEvent      *event;

	TransferType  type;                /* the type of the transfer */
	char         *filename;            /* the name of the save file */
	char         *path;                /* fully qualified local path to this
	                                    * file */

	char         *mime;                /* mime type determined on transfer
	                                    * creation using file name */

	char         *hash;                /* protocol-specific hash to reference
										* this transfer */
	int           active;              /* inactive when first created with 0
										* sources */

	off_t         transmit;            /* total received */
	off_t         transmit_change;     /* same as old but for different purposes
	                                    * NOTE: this is temporary */
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
	struct download
	{
		char         *uniq;            /* unique identifer, see uniq_file in
		                                * download.c */
		char         *state_path;
		unsigned char paused : 1;      /* see SOURCE_PAUSED */
		unsigned char verifying : 1;   /* bla bla, temporary */
	} down;

	/* UPLOAD SPECIFIC */
	struct upload
	{
		/* temporarily hide this transfer from the interface protocol (until its
		 * been completely filled)
		 * NOTE: this is no longer in use */
		unsigned char        display : 1;

		/* special upload (eg authorized by protocol, not giFT) */
		unsigned char        shared : 1;

		/* whether activity on the transfer was seen recently */
		BOOL                 active;

		/* inactivity timeout timer */
		timer_id             inactive_timer;

		/* position in the upload queue */
		struct upload_queue *queue;
	} up;
};
#endif /* EVIL_HASHMAP */

/*****************************************************************************/

/* Init hash map. */
void asp_hashmap_init (void);

/* Free hash map. */
void asp_hashmap_destroy (void);

/* Lookup file name and size by hash. Returns TRUE if an entry was found and
 * sets name and size. Caller must not modify returned name.
 */
as_bool asp_hashmap_lookup (ASHash *hash, char **name, size_t *size);

/* Insert file name and size for given hash. */
void asp_hashmap_insert (ASHash *hash, char *name, size_t size);

/*****************************************************************************/

#endif /* __ASP_HASHMAP_H */
