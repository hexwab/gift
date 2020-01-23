/*
 * $Id: fst_download.h,v 1.4 2003/06/28 20:17:34 beren12 Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#ifndef __FST_DOWNLOAD_H
#define __FST_DOWNLOAD_H

#include "fst_fasttrack.h"
#include "fst_packet.h"
#include "fst_http.h"

/*****************************************************************************/

typedef enum { DownloadNew, DownloadConnecting, DownloadRequesting, DownloadRunning, DownloadComplete } FSTDownloadState;

typedef struct
{
	FSTDownloadState state;

	TCPC *tcpcon;
	FSTPacket *in_packet;		/* input buffer */

	Chunk *chunk;

	/* parsed url */
	unsigned int ip;
	unsigned short port;
	char *uri;

} FSTDownload;

/*****************************************************************************/

/* called by gift to start downloading of a chunk */
int gift_cb_download_start (Protocol *p, Transfer *transfer, Chunk *chunk, Source *source);

/* called by gift to stop download */
void gift_cb_download_stop (Protocol *p, Transfer *transfer, Chunk *chunk, Source *source, int complete);

/* called by gift to remove source */
int gift_cb_source_remove (Protocol *p, Transfer *transfer, Source *source);

/*****************************************************************************/

/* alloc and init download */
FSTDownload *fst_download_create (Chunk *chunk);

/* free download, stop it if necessary */
void fst_download_free (FSTDownload *download);

/* start download */
int fst_download_start (FSTDownload *download);

/* stop download */
int fst_download_stop (FSTDownload *download);

/*****************************************************************************/

#endif /* __FST_DOWNLOAD_H */
