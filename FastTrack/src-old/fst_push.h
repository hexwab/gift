/*
 * $Id: fst_push.h,v 1.1 2003/09/18 14:54:50 mkern Exp $
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

#ifndef __FST_PUSH_H__
#define __FST_PUSH_H__

#include "fst_http_server.h"

/**************************************************************************/

typedef struct
{
	Source *source;				/* source which is firewalled */
	unsigned int push_id;		/* fasttrack push id */
} FSTPush;


typedef struct
{
	List *list;					/* list of requested pushes */

	unsigned int next_push_id;
} FSTPushList;

/*****************************************************************************/

/* called by http server for every received reply */
int fst_push_process_reply (FSTHttpServer *server, TCPC *tcpcon,
							unsigned int push_id);

/*****************************************************************************/

/* alloc and init push */
FSTPush *fst_push_create (Source *source, unsigned int push_id);

/* free push */
void fst_push_free (FSTPush *push);

/* send push request to supernode */
int fst_push_send_request (FSTPush *push, FSTSession *session);

/*****************************************************************************/

/* alloc and init push list */
FSTPushList *fst_pushlist_create ();

/* free push list, frees all pushes */
void fst_pushlist_free (FSTPushList *pushlist);

/*****************************************************************************/

/* add push for source to pushlist if it's not already added,
 * returns already added or new push */
FSTPush *fst_pushlist_add (FSTPushList *pushlist, Source *source);

/* remove push from list, does not free push, return removed push */
FSTPush *fst_pushlist_remove (FSTPushList *pushlist, FSTPush *push);

/*****************************************************************************/

/* lookup push by source */
FSTPush *fst_pushlist_lookup_source (FSTPushList *pushlist, Source *source);

/* lookup push by push id */
FSTPush *fst_pushlist_lookup_id (FSTPushList *pushlist, unsigned int push_id);

/*****************************************************************************/

#endif /* __FST_PUSH_H__ */
