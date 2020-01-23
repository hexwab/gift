/*
 * download.h
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

#ifndef __DOWNLOAD_H
#define __DOWNLOAD_H

/*****************************************************************************/

#include "transfer.h"

/*****************************************************************************/

void       download_report_attached (Connection *c);
Transfer  *download_new             (Connection *c, char *filename,
                                     unsigned long size);
void       download_free            (Transfer *transfer, int premature);
void       download_stop            (Transfer *transfer, int cancel);
void       download_add_source      (Transfer *transfer, Source *source);
void       download_remove_source   (Transfer *transfer, char *location);
void       download_write           (Chunk *chunk, char *segment, size_t len);
void       download_recover_state   ();

/*****************************************************************************/

#endif /* __DOWNLOAD_H */
