/*
 * Copyright (C) 2003 Arend van Beelen jr. (arend@auton.nl)
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

#include "sl_soulseek.h"
#include "sl_download.h"

/*****************************************************************************/

/*****************************************************************************/

// called by gift to start downloading of a chunk
int sl_gift_cb_download_start(Protocol *p, Transfer *transfer, Chunk *chunk, Source *source)
{
	return TRUE;
}

// called by gift to stop download
void sl_gift_cb_download_stop(Protocol *p, Transfer *transfer, Chunk *chunk, Source *source, int complete)
{
}

// called by gift to remove source
void sl_gift_cb_source_remove(Protocol *p, Transfer *transfer, Source *source)
{
}
