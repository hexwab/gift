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

/*****************************************************************************/

List     *upload_list   ();
void      upload_report_attached (Connection *c);
Transfer *upload_new    (Protocol *p, char *host, char *hash, char *filename,
                         char *path, size_t start, size_t stop);
void      upload_free   (Transfer *transfer);
void      upload_stop   (Transfer *upload, int cancel);
void      upload_write  (Chunk *chunk, char *segment, size_t len);

/*****************************************************************************/

#endif /* __UPLOAD_H */
