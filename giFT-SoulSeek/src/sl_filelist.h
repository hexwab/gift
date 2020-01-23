/*
 * Copyright (C) 2003 Felix Nawothnig (felix.nawothnig@t-online.de)
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

#ifndef __SL_FILELIST_H
#define __SL_FILELIST_H

#include "sl_soulseek.h"

/*****************************************************************************/

typedef struct
{
	sl_string *path;
	off_t      size;

	SLMetaAttribute *attributes;
	int              num_attributes;
} SLFile;

/*****************************************************************************/

List *sl_filelist_new_from_packet(SLPacket *packet);

void  sl_filelist_report(List *list, struct if_event *event, SLPeer *peer);

#endif /* __SL_FILELIST_H */
