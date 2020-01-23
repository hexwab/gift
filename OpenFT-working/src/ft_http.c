/*
 * $Id: ft_http.c,v 1.11 2004/03/28 13:55:14 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#include "ft_openft.h"

#include "ft_http.h"

#include <ctype.h>

/*****************************************************************************/

/*
 * This information is according to RFC1738, section 2.2 which was available
 * at the time of this writing at the following RFC1738-compliant URL (*grin*):
 *
 *  http://www.w3.org/Addressing/rfc1738.txt
 *
 * We choose to encode the following possibly reserved characters:
 *
 *  0x26  &
 *  0x3a  :
 *  0x3b  ;
 *  0x3d  =
 *  0x3f  ?
 *  0x40  @
 *
 * Characters with a value of 1 in this table are safe to use unencoded.  Any
 * character with a value of 0, or not defined here, will be encoded.
 */
static unsigned char encode_safe[128] =
{
	/* 0x00 */ 0, /* 0x01 */ 0, /* 0x02 */ 0, /* 0x03 */ 0, /* 0x04 */ 0,
	/* 0x05 */ 0, /* 0x06 */ 0, /* 0x07 */ 0, /* 0x08 */ 0, /* 0x09 */ 0,
	/* 0x0a */ 0, /* 0x0b */ 0, /* 0x0c */ 0, /* 0x0d */ 0, /* 0x0e */ 0,
	/* 0x0f */ 0, /* 0x10 */ 0, /* 0x11 */ 0, /* 0x12 */ 0, /* 0x13 */ 0,
	/* 0x14 */ 0, /* 0x15 */ 0, /* 0x16 */ 0, /* 0x17 */ 0, /* 0x18 */ 0,
	/* 0x19 */ 0, /* 0x1a */ 0, /* 0x1b */ 0, /* 0x1c */ 0, /* 0x1d */ 0,
	/* 0x1e */ 0, /* 0x1f */ 0, /* 0x20 */ 0, /* 0x21 */ 1, /* 0x22 */ 0,
	/* 0x23 */ 0, /* 0x24 */ 1, /* 0x25 */ 0, /* 0x26 */ 0, /* 0x27 */ 1,
	/* 0x28 */ 1, /* 0x29 */ 1, /* 0x2a */ 1, /* 0x2b */ 1, /* 0x2c */ 1,
	/* 0x2d */ 1, /* 0x2e */ 1, /* 0x2f */ 1, /* 0x30 */ 1, /* 0x31 */ 1,
	/* 0x32 */ 1, /* 0x33 */ 1, /* 0x34 */ 1, /* 0x35 */ 1, /* 0x36 */ 1,
	/* 0x37 */ 1, /* 0x38 */ 1, /* 0x39 */ 1, /* 0x3a */ 0, /* 0x3b */ 0,
	/* 0x3c */ 0, /* 0x3d */ 0, /* 0x3e */ 0, /* 0x3f */ 0, /* 0x40 */ 0,
	/* 0x41 */ 1, /* 0x42 */ 1, /* 0x43 */ 1, /* 0x44 */ 1, /* 0x45 */ 1,
	/* 0x46 */ 1, /* 0x47 */ 1, /* 0x48 */ 1, /* 0x49 */ 1, /* 0x4a */ 1,
	/* 0x4b */ 1, /* 0x4c */ 1, /* 0x4d */ 1, /* 0x4e */ 1, /* 0x4f */ 1,
	/* 0x50 */ 1, /* 0x51 */ 1, /* 0x52 */ 1, /* 0x53 */ 1, /* 0x54 */ 1,
	/* 0x55 */ 1, /* 0x56 */ 1, /* 0x57 */ 1, /* 0x58 */ 1, /* 0x59 */ 1,
	/* 0x5a */ 1, /* 0x5b */ 0, /* 0x5c */ 0, /* 0x5d */ 0, /* 0x5e */ 0,
	/* 0x5f */ 1, /* 0x60 */ 0, /* 0x61 */ 1, /* 0x62 */ 1, /* 0x63 */ 1,
	/* 0x64 */ 1, /* 0x65 */ 1, /* 0x66 */ 1, /* 0x67 */ 1, /* 0x68 */ 1,
	/* 0x69 */ 1, /* 0x6a */ 1, /* 0x6b */ 1, /* 0x6c */ 1, /* 0x6d */ 1,
	/* 0x6e */ 1, /* 0x6f */ 1, /* 0x70 */ 1, /* 0x71 */ 1, /* 0x72 */ 1,
	/* 0x73 */ 1, /* 0x74 */ 1, /* 0x75 */ 1, /* 0x76 */ 1, /* 0x77 */ 1,
	/* 0x78 */ 1, /* 0x79 */ 1, /* 0x7a */ 1, /* 0x7b */ 0, /* 0x7c */ 0,
	/* 0x7d */ 0, /* 0x7e */ 0, /* 0x7f */ 0
};

/*****************************************************************************/

char *http_code_string (int code)
{
	char *str;

	/* we are only going to support the subset that OpenFT uses for now */
	switch (code)
	{
	 case 200:     str = "OK";                        break;
	 case 206:     str = "Partial Content";           break;
	 case 403:     str = "Forbidden";                 break;
	 case 404:     str = "Not Found";                 break;
	 case 500:     str = "Internal Server Error";     break;
	 case 501:     str = "Not Implemented";           break;
	 case 503:     str = "Service Unavailable";       break;
	 default:      str = NULL;                        break;
	}

	if (!str)
	{
		FT->DBGFN (FT, "unknown http code %i", code);
		str = "Unknown";
	}

	return str;
}

/*****************************************************************************/

BOOL http_check_sentinel (char *data, size_t len)
{
	int cnt;

	assert (len > 0);
	len--;

	for (cnt = 0; len > 0 && cnt < 2; cnt++)
	{
		if (data[len--] != '\n')
			break;

		/* treat CRLF as LF */
		if (data[len] == '\r')
			len--;
	}

	return (cnt == 2);
}

/*****************************************************************************/

static int dec_value_from_hex (char hex_char)
{
	if (!isxdigit (hex_char))
		return 0;

	if (hex_char >= '0' && hex_char <= '9')
		return (hex_char - '0');

	hex_char = toupper (hex_char);

	return ((hex_char - 'A') + 10);
}

char *http_url_decode (const char *encoded)
{
	char *decoded, *ptr;

	if (!encoded)
		return NULL;

	/* make sure we are using our own memory here ... */
	if (!(ptr = gift_strdup (encoded)))
		return NULL;

	/* save the head */
	decoded = ptr;

	while (*ptr)
	{
		/* handle hexadecimal pairs for non-printable or unsafe characters */
		if (*ptr == '%')
		{
			/*
			 * The standard guarantees that a literal % will be followed by
			 * a hexadecimal pair, but we know better than to trust
			 * untrusted input.  Silently fall through and treat %'s as
			 * though we encountered %25 if they are not properly used.
			 */
			if (isxdigit (ptr[1]) && isxdigit (ptr[2]))
			{
				*ptr  = (dec_value_from_hex (ptr[1]) & 0xf) << 4;
				*ptr |= (dec_value_from_hex (ptr[2]) & 0xf);

				string_move (ptr + 1, ptr + 3);
			}
		}

		ptr++;
	}

	return decoded;
}

char *http_url_encode (const char *unencoded)
{
	String *encoded;
	int     chr;

	if (!unencoded)
		return NULL;

	encoded = string_new (NULL, 0, 0, TRUE);
	assert (encoded != NULL);

	while ((chr = *unencoded) != 0)
	{
		if (chr >= 0 && chr < sizeof (encode_safe) && encode_safe[chr])
			string_appendc (encoded, (char)(chr));
		else
			string_appendf (encoded, "%%%02x", (unsigned char)(chr));

		unencoded++;
	}

	return string_free_keep (encoded);
}

/*****************************************************************************/

static unsigned int http_parse_keylist (Dataset **d, char *data)
{
	char *row;
	char *key;
	char *value;

	if (!d || !data)
		return 0;

	while ((row = string_sep_set (&data, "\r\n")))
	{
		if (!(key = string_sep (&row, ": ")) || !row)
			continue;

		value = row;

		dataset_insertstr (d, key, value);
	}

	return dataset_length (*d);
}

/*****************************************************************************/

FTHttpReply *ft_http_reply_new (int code)
{
	FTHttpReply *reply;

	if (!(reply = MALLOC (sizeof (FTHttpReply))))
		return NULL;

	reply->version = 1.0;              /* HTTP/1.0 by default */
	reply->keylist = dataset_new (DATASET_LIST);
	reply->code    = code;

	return reply;
}

void ft_http_reply_free (FTHttpReply *reply)
{
	if (!reply)
		return;

	dataset_clear (reply->keylist);
	free (reply);
}

FTHttpReply *ft_http_reply_unserialize (char *data)
{
	FTHttpReply *reply;
	char        *resp;
	int          vermaj;
	int          vermin;
	int          code;

	assert (data != NULL);

	/* should be something like: HTTP/1.0 200 OK\r\n..., we want just that
	 * first line here */
	if (!(resp = string_sep_set (&data, "\r\n")))
		return NULL;

	/* examine the first line (resp) closer */
	                       string_sep (&resp, "/");   /* shift past HTTP/ */
	vermaj = gift_strtoul (string_sep (&resp, "."));  /* shift past 1. */
	vermin = gift_strtoul (string_sep (&resp, " "));  /* shift past 0  */
	code   = gift_strtoul (string_sep (&resp, " "));  /* shift past 200 */

	if (!(reply = ft_http_reply_new (code)))
		return NULL;

	/* override the default version of 1.0 */
	reply->version = ((float)vermaj) + ((float)vermin / 10.0);

	/* parse the rest (the key/value list) */
	http_parse_keylist (&reply->keylist, data);

	return reply;
}

static void add_keylist (ds_data_t *key, ds_data_t *value, String *str)
{
	string_appendf (str, "%s: %s\r\n", key->data, value->data);
}

char *ft_http_reply_serialize (FTHttpReply *reply, size_t *retlen)
{
	String     *str;
	const char *codestr;

	if (!reply)
		return NULL;

	codestr = http_code_string (reply->code);
	assert (codestr != NULL);

	if (!(str = string_new (NULL, 0, 0, TRUE)))
		return NULL;

	string_appendf (str, "HTTP/%.01f %i %s\r\n",
	                reply->version, reply->code, codestr);
	dataset_foreach (reply->keylist, DS_FOREACH(add_keylist), str);
	string_appendf (str, "\r\n");

	if (retlen)
		*retlen = str->len;

	return string_free_keep (str);
}

int ft_http_reply_send (FTHttpReply *reply, TCPC *c)
{
	char  *data;
	size_t len;
	int    ret;

	data = ft_http_reply_serialize (reply, &len);

	/* as promised, we will handle memory management */
	ft_http_reply_free (reply);

	if (!data)
		return -1;

	ret = tcp_write (c, (unsigned char *)data, len);
	free (data);

	return ret;
}

/*****************************************************************************/

FTHttpRequest *ft_http_request_new (const char *method, const char *request)
{
	FTHttpRequest *req;

	if (!(req = MALLOC (sizeof (FTHttpRequest))))
		return NULL;

	req->method = strdup (method);
	assert (req->method != NULL);

	req->request = strdup (request);
	assert (req->request != NULL);

	req->keylist = dataset_new (DATASET_LIST);
	assert (req->keylist != NULL);

	return req;
}

void ft_http_request_free (FTHttpRequest *req)
{
	if (!req)
		return;

	dataset_clear (req->keylist);
	free (req->method);
	free (req->request);
	free (req);
}

FTHttpRequest *ft_http_request_unserialize (char *data)
{
	FTHttpRequest *req;
	char          *resp;
	char          *method;
	char          *request;

	assert (data != NULL);

	/* data should begin with METHOD requesturi HTTP/version\r\n */
	if (!(resp = string_sep_set (&data, "\r\n")))
		return NULL;

	method  = string_sep (&resp, " ");    /* METHOD */
	request = string_sep (&resp, " ");    /* requesturi */
	                                      /* [ version is not important ] */

	if (method == NULL || request == NULL)
		return NULL;

	if (!(req = ft_http_request_new (method, request)))
		return NULL;

	http_parse_keylist (&req->keylist, data);

	return req;
}

char *ft_http_request_serialize (FTHttpRequest *req, size_t *retlen)
{
	String *str;

	if (!req)
		return NULL;

	if (!(str = string_new (NULL, 0, 0, TRUE)))
		return NULL;

	string_appendf (str, "%s %s HTTP/1.0\r\n", req->method, req->request);
	dataset_foreach (req->keylist, DS_FOREACH(add_keylist), str);
	string_appendf (str, "\r\n");

	if (retlen)
		*retlen = str->len;

	return string_free_keep (str);
}

int ft_http_request_send (FTHttpRequest *req, TCPC *c)
{
	char  *data;
	size_t len;
	int    ret;

	data = ft_http_request_serialize (req, &len);

	/* as promised, we will handle memory management */
	ft_http_request_free (req);

	if (!data)
		return -1;

	ret = tcp_write (c, (unsigned char *)data, len);
	free (data);

	return ret;
}

/*****************************************************************************/

#if 0
int main (int argc, char **argv)
{
	char *foo;

	foo = http_url_encode ("this is a !(*#@$*#@)$UJOKJ test");
	assert (foo != NULL);

	printf ("foo = '%s'\n", foo);

	free (foo);

	return 0;
}
#endif
