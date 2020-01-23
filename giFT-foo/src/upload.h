/*
 * $Id: upload.h,v 1.30 2003/06/26 09:24:21 jasta Exp $
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

#ifndef __UPLOAD_H
#define __UPLOAD_H

/*****************************************************************************/

#include "plugin/share.h"
#include "transfer.h"
#include "lib/tcpc.h"

/*****************************************************************************/

List      *upload_list    ();
void       upload_report_attached (TCPC *c);
void       upload_display (Transfer *transfer);
Transfer  *upload_new     (Protocol *p, char *host, char *htype, char *hash,
                           char *filename, char *path, off_t start, off_t stop,
                           int display, int shared);
void       upload_stop    (Transfer *upload, int cancel);
void       upload_write   (Chunk *chunk, unsigned char *segment, size_t len);
int        upload_length  (char *user);

int        upload_auth    (const char *user, Share *share);

void       upload_disable ();
void       upload_enable  ();
int        upload_status  ();
unsigned long upload_availability ();
size_t     upload_throttle (Chunk *chunk, size_t len);

/*****************************************************************************/

#endif /* __UPLOAD_H */
