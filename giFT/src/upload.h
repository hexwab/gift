/*
 * $Id: upload.h,v 1.35 2004/03/22 17:06:06 mkern Exp $
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

List      *upload_list    (void);
void       upload_report_attached (TCPC *c);
void       upload_display (Transfer *transfer);
Transfer  *upload_new     (Protocol *p, char *user, char *hash,
                           char *filename, char *path, off_t start, off_t stop,
                           int display, int shared);
void       upload_stop    (Transfer *upload, int cancel);
void       upload_write   (Chunk *chunk, unsigned char *segment, size_t len);
int        upload_length  (char *user);

int        upload_auth    (Protocol *p, const char *user, Share *share,
                           upload_auth_t *info);

/**
 * Disable uploading and hide shares.
 */
void       upload_disable (void);

/**
 * Enable uploading and show shares.
 */
void       upload_enable  (void);

/**
 * Get upload status.
 *
 * @retval -1 Shares are hidden.
 * @retval  0 Sharing without upload limit.
 * @retval >0 Sharing with max retval uploads.
 */
int        upload_status  (void);

GIFTD_EXPORT
  unsigned long upload_availability (void); /* erroneoously used by Gnutella */

GIFTD_EXPORT
  size_t upload_throttle (Chunk *chunk, size_t len);

/*****************************************************************************/

#endif /* __UPLOAD_H */
