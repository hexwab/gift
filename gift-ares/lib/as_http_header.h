/*
 * $Id: as_http_header.h,v 1.1 2004/09/03 16:18:14 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_HTTP_HEADER_H
#define __AS_HTTP_HEADER_H

/*****************************************************************************/

typedef enum
{
	HTHD_REQUEST = 0,
	HTHD_REPLY
} ASHttpHeaderType;

typedef enum
{
	HTHD_VER_10 = 0,
	HTHD_VER_11
} ASHttpVersion;

typedef enum
{
	HTHD_GET = 0,
	HTHD_HEAD,
	HTHD_GIVE		/* special method by fasttrack push replies */
} ASHttpMethod;

typedef struct
{
	ASHttpHeaderType type;	

	ASHttpVersion version;
	ASHashTable *fields;

	/* request */
	ASHttpMethod method;
	char *uri;

	/* reply */
	int code;
	char *code_str;		/* contains actual code str in case of parsed reply */

} ASHttpHeader;

/*****************************************************************************/

/* alloc and init header for request */
ASHttpHeader *as_http_header_request (ASHttpVersion version,
                                      ASHttpMethod method, char *uri);

/* alloc and init header for reply */
ASHttpHeader *as_http_header_reply (ASHttpVersion version, int code);

/* alloc and init header from recveid data,
 * returns NULL if header is incomplete,
 * data_len is set to length of the header in data if parsing succeeds
 */
ASHttpHeader *as_http_header_parse (char *data, int *data_len);

/* free header */
void as_http_header_free (ASHttpHeader *header);

/* free header, set *header to NULL */
void as_http_header_free_null (ASHttpHeader **header);

/*****************************************************************************/

/* get header field, do not modify/free returned string! */
char *as_http_header_get_field (ASHttpHeader *header, char *name);

/* set header field */
void as_http_header_set_field (ASHttpHeader *header, char *name, char *value);

/*****************************************************************************/

/* compile header, caller frees returned libgift string */
String *as_http_header_compile (ASHttpHeader *header);

/*****************************************************************************/

/* return static string explaining http reply code */
char *as_http_code_str (int code);

/*****************************************************************************/

#endif /* __AS_HTTP_HEADER_H */
