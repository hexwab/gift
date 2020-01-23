/*
 * $Id: fst_http_header.h,v 1.1 2003/09/10 11:10:25 mkern Exp $
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

#ifndef __FST_HTTP_HEADER_H
#define __FST_HTTP_HEADER_H

#include <libgift/dataset.h>
#include <libgift/strobj.h>

/*****************************************************************************/

typedef enum
{
	HTHD_REQUEST = 0,
	HTHD_REPLY
} FSTHttpHeaderType;

typedef enum
{
	HTHD_VER_10 = 0,
	HTHD_VER_11
} FSTHttpVersion;

typedef enum
{
	HTHD_GET = 0,
	HTHD_HEAD,
	HTHD_GIVE		/* special method by fasttrack push replies */
} FSTHttpMethod;

typedef struct
{
	FSTHttpHeaderType type;	

	FSTHttpVersion version;
	Dataset *fields;

	/* request */
	FSTHttpMethod method;
	char *uri;

	/* reply */
	int code;
	char *code_str;		/* contains actual code str in case of parsed reply */

} FSTHttpHeader;

/*****************************************************************************/

/* alloc and init header for request */
FSTHttpHeader *fst_http_header_request (FSTHttpVersion version,
										FSTHttpMethod method, char *uri);

/* alloc and init header for reply */
FSTHttpHeader *fst_http_header_reply (FSTHttpVersion version, int code);

/* alloc and init header from recveid data,
 * returns NULL if header is incomplete,
 * data_len is set to length of the header in data if parsing succeeds
 */
FSTHttpHeader *fst_http_header_parse (char *data, int *data_len);

/* free header */
void fst_http_header_free (FSTHttpHeader *header);

/* free header, set *header to NULL */
void fst_http_header_free_null (FSTHttpHeader **header);

/*****************************************************************************/

/* get header field, do not modify/free returned string! */
char *fst_http_header_get_field (FSTHttpHeader *header, char *name);

/* set header field */
void fst_http_header_set_field (FSTHttpHeader *header, char *name, char *value);

/*****************************************************************************/

/* compile header, caller frees returned libgift string */
String *fst_http_header_compile (FSTHttpHeader *header);

/*****************************************************************************/

/* return static string explaining http reply code */
char *fst_http_code_str (int code);

/*****************************************************************************/

#endif /* __FST_HTTP_HEADER_H */
