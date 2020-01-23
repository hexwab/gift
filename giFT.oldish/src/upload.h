/*
 * upload.h
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

#ifndef __UPLOAD_H
#define __UPLOAD_H

/*****************************************************************************/

#include "transfer.h"
#include "connection.h"

/*****************************************************************************/

typedef enum
{
	AUTH_ACCEPTED = 0,                 /* success */
	AUTH_MAX_PERUSER,                  /* maximum transfers per user reached */
	AUTH_MAX,                          /* maximum total transfer reached */
	AUTH_STALE,                        /* share is stale, will be refreshed
	                                    * in giFT space and may be retried at
	                                    * an unspecified time in the future */
	AUTH_INVALID,                      /* share is invalid */
} AuthReason;

/*****************************************************************************/

List      *upload_list    ();
void       upload_report_attached (Connection *c);
void       upload_display (Transfer *transfer);
Transfer  *upload_new     (Protocol *p, char *host, char *hash, char *filename,
                           char *path, off_t start, off_t stop,
                           int display, int shared);
void       upload_stop    (Transfer *upload, int cancel);
void       upload_write   (Chunk *chunk, unsigned char *segment, size_t len);
int        upload_length  (char *user);

FileShare *upload_auth    (char *user, char *path, AuthReason *reason);
void       upload_disable ();
void       upload_enable  ();
int        upload_status  ();
unsigned long upload_availability ();

/*****************************************************************************/

#endif /* __UPLOAD_H */
