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

typedef enum _transfer_type
{
	TRANSFER_DOWNLOAD,
	TRANSFER_UPLOAD
} TransferType;

typedef struct _transfer
{
	IFEventID     id;

	TransferType  type;     /* the type of the transfer */
	char         *filename; /* the name of the file */
	char         *path;     /* fully qualified local path to this file */

	int           active;   /* inactive when first created with 0 sources */

	unsigned long transmit; /* total received */
	unsigned long total;    /* total filesize to download */
	unsigned long max_seek; /* current filesize according to the filesystem */

	List         *chunks;   /* partitioned download segments */
	List         *sources;  /* sources used for downloading */

	unsigned long timer;    /* logs the download progress in the resume file */

	FILE         *f;        /* read/write file descriptor */

	/* DOWNLOAD SPECIFIC */

	char         *state_path;

	/* UPLOAD SPECIFIC */


} Transfer;

/*****************************************************************************/

typedef struct _source
{
	Protocol *p;             /* p->name = OpenFT */

	char     *user;          /* 65.4.102.175                           */
	char     *hash;          /* 3da9f138de951c22c8a850899680a9e4       */
	char     *url;           /* OpenFT://65.4.102.175:1216/file.tar.gz */

	int       active;        /* currently being used */
} Source;

/*****************************************************************************/

typedef struct _chunk
{
	Transfer     *transfer;  /* parent */
	Source       *source;    /* data source */

	int           active;    /* inactive chunks have been divided, but no
	                          * source is actively downloading */

	unsigned long transmit;  /* currently received */
	unsigned long tmp_recv;  /* k/s calculations, timeout */
	unsigned long start;
	unsigned long stop;

	int           timeout;   /* timeout */

	void         *data;      /* protocol-specific data */
} Chunk;

/*****************************************************************************/

Transfer  *transfer_new  (TransferType direction, char *filename,
                          unsigned long size);
void       transfer_free (Transfer *transfer);
void       transfer_stop (Transfer *transfer, int cancel);

/*****************************************************************************/

Source    *source_new    (char *user, char *hash, char *url);
void       source_free   (Source *source);
int        source_equal  (Source *a, Source *b);

/*****************************************************************************/

Chunk     *chunk_new     (Transfer *transfer, Source *source,
                          size_t start, size_t stop);
void       chunk_write   (Chunk *chunk, char *segment, size_t len);
void       chunk_free    (Chunk *chunk);
void       chunk_cancel  (Chunk *chunk);

/*****************************************************************************/

#endif /* __TRANSFER_H */
