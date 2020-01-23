/*
 * $Id: as_http_header.c,v 1.5 2004/11/20 03:02:54 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* alloc and init header for request */
ASHttpHeader *as_http_header_request (ASHttpVersion version,
                                      ASHttpMethod method, char *uri)
{
	ASHttpHeader *header = malloc (sizeof (ASHttpHeader));

	if (!header)
		return NULL;

	header->type = HTHD_REQUEST;

	header->version = version;
	header->fields = as_hashtable_create_mem (TRUE); /* copy keys */

	/* request */
	header->method = method;
	header->uri = strdup (uri);

	/* reply */
	header->code = 0;
	header->code_str = NULL;

	return header;
}

/* alloc and init header for reply */
ASHttpHeader *as_http_header_reply (ASHttpVersion version, int code)
{
	ASHttpHeader *header = malloc (sizeof (ASHttpHeader));

	if (!header)
		return NULL;

	header->type = HTHD_REPLY;

	header->version = version;
	header->fields = as_hashtable_create_mem (TRUE); /* copy keys */

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
ASHttpHeader *as_http_header_parse (char *data, int *data_len)
{
	ASHttpHeader *header;
	char *p, *line, *curr;
	int i, len = 0;
	as_bool ret;

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
	}

	if (!len)
		return NULL;

	/* create \0 terminated working copy */
	if (! (data = curr = gift_strndup (data, len)))
		return NULL;

	/* alloc header struct */
	if (! (header = malloc (sizeof (ASHttpHeader))))
	{
		free (data);
		return NULL;
	}

	header->fields = as_hashtable_create_mem (TRUE); /* copy keys */

	/* defaults */
	header->method = HTHD_GET;
	header->uri = NULL;
	header->code = 0;
	header->code_str = NULL;

	/* parse first line */
	if (! (line = string_sep (&curr, "\r\n")))
	{
		free (data);
		as_http_header_free (header);
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
			as_http_header_free (header);
			return NULL;
		}

		header->version = (! strcmp (p, "HTTP/1.1")) ? HTHD_VER_11 : HTHD_VER_10;

		/* get code */
		if((p = string_sep (&line, " ")) == NULL || line == NULL)
		{
			/* invalid header */
			free (data);
			as_http_header_free (header);
			return NULL;
		}

		header->code = gift_strtol (p);

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
			as_http_header_free (header);
			return NULL;
		}

		/* get uri */
		if ((p = string_sep (&line, " ")) == NULL || line == NULL)
		{
			/* invalid header */
			free (data);
			as_http_header_free (header);
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
		ret = as_hashtable_insert_str (header->fields, p, strdup (line));
		assert (ret);
	}

	free (data);
	*data_len = len;

	return header;
}

/* free header */
void as_http_header_free (ASHttpHeader *header)
{
	if (!header)
		return;

	as_hashtable_free (header->fields, TRUE); /* free values */
	free (header->uri);
	free (header->code_str);

	free (header);
}


void as_http_header_free_null (ASHttpHeader **header)
{
	if (!header || ! *header)
		return;

	as_http_header_free (*header);
	*header = NULL;
}


/*****************************************************************************/

/* get header field, do not modify/free returned string! */
char *as_http_header_get_field (ASHttpHeader *header, char *name)
{
	char *value, *low_name;

	assert (header && name);

	if (!header || !name)
		return NULL;

	low_name = strdup (name);
	string_lower (low_name);
	value = as_hashtable_lookup_str (header->fields, low_name);
	free (low_name);

	return value;
}

/* set header field */
void as_http_header_set_field (ASHttpHeader *header, char *name, char *value)
{
	as_bool ret;

	assert (header && name && value);

	if (!header || !name || !value)
		return;

	ret = as_hashtable_insert_str (header->fields, name, strdup (value));
	assert (ret);
}

/*****************************************************************************/

static as_bool http_header_compile_field (ASHashTableEntry *entry, 
                                          String *str)
{
	string_appendf (str, "%s: %s\r\n", (char*)entry->key, (char*)entry->val);

	return FALSE; /* keep entry */
}

/* compile header, caller frees returned libgift string */
String *as_http_header_compile (ASHttpHeader *header)
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
		char *code_str = header->code_str ? header->code_str : as_http_code_str (header->code);

		string_appendf (str, "HTTP/%s %d %s\r\n",
						version_str, header->code, code_str);
	}
	else
	{
		return NULL;
	}

	/* add headers */
	as_hashtable_foreach (header->fields,
	                      (ASHashTableForeachFunc) http_header_compile_field,
                          (void *)str);

	/* add empty line for header termination */
	string_append (str, "\r\n");

	return str;
}

/*****************************************************************************/

/* return static string explaining http reply code */
char *as_http_code_str (int code)
{
	char *str;

	switch (code)
	{
	case 200: str = "OK";                        break;
	case 206: str = "Partial Content";           break;
	case 400: str = "Bad Request";               break;
	case 403: str = "Forbidden";                 break;
	case 404: str = "Not Found";                 break;
	case 500: str = "Internal Server Error";     break;
	case 501: str = "Not Implemented";           break;
	case 503: str = "Service Unavailable";       break;
	default:  str = "<Unknown HTTP reply code>"; break;
	}

	return str;
}

/*****************************************************************************/
