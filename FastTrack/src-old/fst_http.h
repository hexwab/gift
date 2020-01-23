/*
 * $Id: fst_http.h,v 1.4 2003/07/04 03:54:45 beren12 Exp $
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

#ifndef __FST_HTTP_H
#define __FST_HTTP_H

#include <libgift/dataset.h>
#include "fst_packet.h"

/*****************************************************************************/

typedef struct
{
	char *method;
	char *uri;

	Dataset *headers;

} FSTHttpRequest;

typedef struct
{
	int code;
	char *code_str;

	Dataset *headers;

} FSTHttpReply;

/*****************************************************************************/

/* alloc and init request */
FSTHttpRequest *fst_http_request_create (char *method, char *uri);

/* free request */
void fst_http_request_free (FSTHttpRequest *request);

/* add header to request */
void fst_http_request_set_header (FSTHttpRequest *request, char *name, char *value);

/* compile request and append it to packet */
int fst_http_request_compile (FSTHttpRequest *request, FSTPacket *packet);

/*****************************************************************************/

/* alloc an init reply */
FSTHttpReply *fst_http_reply_create ();

/* free reply */
void fst_http_reply_free (FSTHttpReply *reply);

/* retrieve header, do not modify/free returned string! */
char *fst_http_reply_get_header (FSTHttpReply *reply, char *name);

/* parses reply and moves packet->read_ptr to first byte of http body */
int fst_http_reply_parse (FSTHttpReply *reply, FSTPacket *packet);

/*****************************************************************************/

#endif /* __FST_HTTP_H */
