/*
 * $Id: fst_http_header.c,v 1.5 2004/06/19 19:53:55 mkern Exp $
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
#include "fst_http_header.h"

/*****************************************************************************/

/* alloc and init header for request */
FSTHttpHeader *fst_http_header_request (FSTHttpVersion version,
										FSTHttpMethod method, char *uri)
{
	FSTHttpHeader *header = malloc (sizeof (FSTHttpHeader));

	if (!header)
		return NULL;

	header->type = HTHD_REQUEST;

	header->version = version;
	header->fields = dataset_new (DATASET_HASH);

	/* request */
	header->method = method;
	header->uri = strdup (uri);

	/* reply */
	header->code = 0;
	header->code_str = NULL;

	return header;
}

/* alloc and init header for reply */
FSTHttpHeader *fst_http_header_reply (FSTHttpVersion version, int code)
{
	FSTHttpHeader *header = malloc (sizeof (FSTHttpHeader));

	if (!header)
		return NULL;

	header->type = HTHD_REPLY;

	header->version = version;
	header->fields = dataset_new (DATASET_HASH);

	/* request */
	header->method = HTHD_GET;
	header->uri = NULL;

	/* reply */
	header->code = code;
	header->code_str = NULL;

	return header;
}

/* alloc and init header from recveid data,
 * returns NULL if header is incomplete,
 * data_len is set to length of the header in data if parsing succeeds
 */
FSTHttpHeader *fst_http_header_parse (char *data, int *data_len)
{
	FSTHttpHeader *header;
	char *p, *line, *curr;
	int i, len = 0;

	/* check if header is complete
	 * this is probably the worst piece of code i've ever written
	 */
	for (i=0,p=data; i<=(*data_len)-3 && *p; i++,p++)
	{
		if (p[0] != '\r' || p[1] != '\n')
			continue;

		if (i <= (*data_len)-4 && p[2] == '\r' && p[3] == '\n')
		{
			len = i + 4; 
			break;
		}

		/* kazaa weirdness */
		if (p[2] == '\n')
		{
			len = i + 3; 
			break;
		}
	}

	if (!len)
		return NULL;

	/* create \0 terminated working copy */
	if (! (data = curr = gift_strndup (data, len)))
		return NULL;

	/* alloc header struct */
	if (! (header = malloc (sizeof (FSTHttpHeader))))
	{
		free (data);
		return NULL;
	}

	header->fields = dataset_new (DATASET_HASH);

	/* defaults */
	header->method = HTHD_GET;
	header->uri = NULL;
	header->code = 0;
	header->code_str = NULL;

	/* parse first line */
	if (! (line = string_sep (&curr, "\r\n")))
	{
		free (data);
		fst_http_header_free (header);
		return NULL;
	}

	if (strncmp (line, "HTTP", 4) == 0)
	{
		/* http reply */
		header->type = HTHD_REPLY;

		/* get version */
		if((p = string_sep (&line, " ")) == NULL || line == NULL)
		{
			/* invalid header */
			free (data);
			fst_http_header_free (header);
			return NULL;
		}

		header->version = (! strcmp (p, "HTTP/1.1")) ? HTHD_VER_11 : HTHD_VER_10;

		/* get code */
		if((p = string_sep (&line, " ")) == NULL || line == NULL)
		{
			/* invalid header */
			free (data);
			fst_http_header_free (header);
			return NULL;
		}

		header->code = ATOI (p);

		/* get code string */
		header->code_str = strdup (line);
	}
	else
	{
		/* http request */
		header->type = HTHD_REQUEST;

		/* get method */
		p = string_sep (&line, " ");
		if      (p && line && !strcmp (p, "GET" )) header->method = HTHD_GET;
		else if (p && line && !strcmp (p, "HEAD")) header->method = HTHD_HEAD;
		else if (p && line && !strcmp (p, "GIVE")) header->method = HTHD_GIVE;
		else
		{
			/* unknown method */
			free (data);
			fst_http_header_free (header);
			return NULL;
		}

		/* get uri */
		if ((p = string_sep (&line, " ")) == NULL || line == NULL)
		{
			/* invalid header */
			free (data);
			fst_http_header_free (header);
			return NULL;
		}

		header->uri = strdup (p);

		/* get version */
		header->version = (!strcmp (line, "HTTP/1.1")) ? HTHD_VER_11 : HTHD_VER_10;
	}

	/* parse header fields */
	while ( (line = string_sep (&curr, "\r\n")))
	{
		p = string_sep (&line, ": ");

		if (!p || !line)
			continue;

		string_lower (p);
		dataset_insertstr (&header->fields, p, line);
	}

	free (data);
	*data_len = len;

	return header;
}

/* free header */
void fst_http_header_free (FSTHttpHeader *header)
{
	if (!header)
		return;

	dataset_clear (header->fields);
	free (header->uri);
	free (header->code_str);

	free (header);
}


void fst_http_header_free_null (FSTHttpHeader **header)
{
	if (!header || ! *header)
		return;

	fst_http_header_free (*header);
	*header = NULL;
}


/*****************************************************************************/

/* get header field, do not modify/free returned string! */
char *fst_http_header_get_field (FSTHttpHeader *header, char *name)
{
	char *value, *low_name;

	if (!header)
		return NULL;

	low_name = strdup (name);
	string_lower (low_name);
	value = dataset_lookupstr (header->fields, low_name);
	free (low_name);

	return value;
}

/* set header field */
void fst_http_header_set_field (FSTHttpHeader *header, char *name, char *value)
{
	if (!header)
		return;

	dataset_insertstr (&header->fields, name, value);
}

/*****************************************************************************/

static void http_header_compile_field (ds_data_t *key, ds_data_t *value,
									   String *str)
{
	string_appendf (str, "%s: %s\r\n", (char*)key->data, (char*)value->data);
}

/* compile header, caller frees returned libgift string */
String *fst_http_header_compile (FSTHttpHeader *header)
{
	String *str;

	if (!header)
		return NULL;

	if (! (str = string_new (NULL, 0, 0, TRUE)))
		return NULL;

	/* compile first line */
	if (header->type == HTHD_REQUEST)
	{
		char *version_str = (header->version == HTHD_VER_11) ? "1.1" : "1.0";
		char *method_str = NULL;

		switch (header->method)
		{
		case HTHD_GET:	method_str = "GET";		break;
		case HTHD_HEAD:	method_str = "HEAD";	break;
		case HTHD_GIVE:	method_str = "GIVE";	break;
		}

		string_appendf (str, "%s %s HTTP/%s\r\n",
						method_str, header->uri, version_str);
	}
	else if (header->type == HTHD_REPLY)
	{
		char *version_str = (header->version == HTHD_VER_11) ? "1.1" : "1.0";
		char *code_str = header->code_str ? header->code_str : fst_http_code_str (header->code);

		string_appendf (str, "HTTP/%s %d %s\r\n",
						version_str, header->code, code_str);
	}
	else
	{
		return NULL;
	}

	/* add headers */
	dataset_foreach (header->fields, DS_FOREACH(http_header_compile_field),
					 (void*)str);

	/* add empty line for header termination */
	string_append (str, "\r\n");

	return str;
}

/*****************************************************************************/

/* return static string explaining http reply code */
char *fst_http_code_str (int code)
{
	char *str;

	/* ripped from OpenFT */
	switch (code)
	{
	case 200:     str = "OK";                        break;
	case 206:     str = "Partial Content";           break;
	case 400:     str = "Bad Request";               break;
	case 403:     str = "Forbidden";                 break;
	case 404:     str = "Not Found";                 break;
	case 500:     str = "Internal Server Error";     break;
	case 501:     str = "Not Implemented";           break;
	case 503:     str = "Service Unavailable";       break;
	default:      str = "<Unknown HTTP reply code>"; break;
	}

	return str;
}

/*****************************************************************************/
