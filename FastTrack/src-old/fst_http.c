/*
 * $Id: fst_http.c,v 1.4 2003/07/04 03:54:45 beren12 Exp $
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

#include "fst_fasttrack.h"
#include "fst_http.h"

/*****************************************************************************/

/* alloc and init request */
FSTHttpRequest *fst_http_request_create (char *method, char *uri)
{
	FSTHttpRequest *request = malloc (sizeof (FSTHttpRequest));

	request->method = strdup (method);
	request->uri = strdup (uri);
	request->headers = dataset_new (DATASET_HASH);

	return request;
}

/* free request */
void fst_http_request_free (FSTHttpRequest *request)
{
	if (!request)
		return;

	free (request->method);
	free (request->uri);
	dataset_clear (request->headers);

	free (request);
}

/* add header to request */
void fst_http_request_set_header (FSTHttpRequest *request, char *name, char *value)
{
	dataset_insertstr (&request->headers, name, value);
}

void http_reply_compile_header (ds_data_t *key, ds_data_t *value, FSTPacket *packet)
{
	char *line = stringf ("%s: %s\r\n", (char*)key->data, (char*)value->data);
	fst_packet_put_ustr (packet, line, strlen (line));
}

/* compile request and append it to packet */
int fst_http_request_compile (FSTHttpRequest *request, FSTPacket *packet)
{
	char *line;

	/* compile first line */
	line = malloc (strlen (request->uri) + strlen (request->method) + 16);
	sprintf (line, "%s %s HTTP/1.1\r\n", request->method, request->uri);
	fst_packet_put_ustr (packet, line, strlen (line));
	free (line);

	/* add headers */
	dataset_foreach (request->headers, DS_FOREACH(http_reply_compile_header), (void*)packet);

	/* add empty line for header termination */
	fst_packet_put_ustr (packet, "\r\n", 2);

	return TRUE;
}

/*****************************************************************************/

/* alloc an init reply */
FSTHttpReply *fst_http_reply_create ()
{
	FSTHttpReply *reply = malloc (sizeof (FSTHttpReply));

	reply->code = -1;
	reply->code_str = NULL;
	reply->headers = NULL;

	return reply;
}

/* free reply */
void fst_http_reply_free (FSTHttpReply *reply)
{
	if (!reply)
		return;

	if (reply->code_str)
		free (reply->code_str);
	dataset_clear (reply->headers);

	free (reply);
}

/* retrieve header, do not modify/free returned string! */
char *fst_http_reply_get_header (FSTHttpReply *reply, char *name)
{
	char *value, *low_name;

	if (!reply->headers)
		return NULL;

	low_name = strdup (name);
	string_lower (low_name);
	value = dataset_lookupstr (reply->headers, low_name);
	free (low_name);

	return value;
}

/* parses reply and moves packet->read_ptr to first byte of http body */
int fst_http_reply_parse (FSTHttpReply *reply, FSTPacket *packet)
{
	char *header, *tmp, *p, *line;
	int i, len;

	/* free previously used stuff */
	dataset_clear (reply->headers);
	reply->headers = dataset_new (DATASET_HASH);
	if (reply->code_str)
	{
		free (reply->code_str);
		reply->code_str = NULL;
	}

	/* check if packet contains entire header */
	p = packet->read_ptr;
	len = fst_packet_remaining(packet) - 2;

	/* what a mess :-p */
	for(i = 0; ; i++, p++)
	{
		if (p[0] == '\r' && p[1] == '\n')
		{
			if (p[2] == 0x0a)	/* "\r\n\n", kazaa weirdness */
			{
				i += 3;
				break;
			}
			if (len - i >= 2 && p[2] == '\r' && p[3] == '\n')
			{
				i += 4;
				break;
			}
		}
		if (i == len)
			return FALSE;
	}

	/* create working copy of header */
	header = tmp = fst_packet_get_str (packet, i);

	/* parse first line */
	if ( (line = string_sep_set (&tmp, "\r\n")))
	{
		string_sep (&line, " ");						/* shift past HTTP/1.1 */
		reply->code = ATOI (string_sep (&line, " "));	/* shift past 200 */
		reply->code_str = strdup (line);
	}

	/* parse header fields */
	while ( (line = string_sep_set (&tmp, "\r\n")))
	{
		p = string_sep (&line, ": ");

		if (!p || !line)
			continue;

		string_lower (p);
		dataset_insertstr (&reply->headers, p, line);
	}

	free (header);

	return TRUE;
}

/*****************************************************************************/
