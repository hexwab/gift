/*
 * $Id: fst_utils.c,v 1.5 2003/07/04 19:36:47 mkern Exp $
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

/*****************************************************************************/

void print_bin_data(unsigned char * data, int len)
{
	int i;
	int i2;
	int i2_end;

//	printf("data len %d\n", data_len);
	fprintf(stderr, "\nbinary data\n");

	for (i2 = 0; i2 < len; i2 = i2 + 16)
	{
		i2_end = (i2 + 16 > len) ? len: i2 + 16;
		for (i = i2; i < i2_end; i++)
			if (isprint(data[i]))
				fprintf(stderr, "%c", data[i]);
			else
			fprintf(stderr, ".");
		for ( i = i2_end ; i < i2 + 16; i++)
			fprintf(stderr, " ");
		fprintf(stderr, " | ");
		for (i = i2; i < i2_end; i++)
			fprintf(stderr, "%02x ", data[i]);
		fprintf(stderr, "\n");
	}
}
 
void save_bin_data(unsigned char * data, int len)
{
	register int i;
	register int i2;
	register int i2_end;
	static FILE *logfile;
	
	if(!logfile)
		if((logfile=fopen("ft.log","w"))==NULL)
		{
			perror("cant open logfile");
			exit(1);
		}
//	printf("data len %d\n", len);
	fprintf(logfile,"binary data\r\n");

	for (i2 = 0; i2 < len; i2 = i2 + 16)
	{
		i2_end = (i2 + 16 > len) ? len: i2 + 16;
		for (i = i2; i < i2_end; i++)
			if (isprint(data[i]))
				fprintf(logfile,"%c", data[i]);
			else
			fprintf(logfile,".");
		for ( i = i2_end ; i < i2 + 16; i++)
			fprintf(logfile," ");
		fprintf(logfile," | ");
		for (i = i2; i < i2_end; i++)
			fprintf(logfile,"%02x ", data[i]);
		fprintf(logfile,"\r\n");
	}
}

/*****************************************************************************/
/* url decode/encode helpers */

static int oct_value_from_hex (char hex_char)
{
	if (!isxdigit (hex_char))
		return 0;

	if (hex_char >= '0' && hex_char <= '9')
		return (hex_char - '0');

	hex_char = toupper (hex_char);

	return ((hex_char - 'A') + 10);
}

// caller frees returned string
char *fst_utils_url_decode (char *encoded)
{
	char *decoded, *ptr;

	if (!encoded)
		return NULL;

	/* make sure we are using our own memory here ... */
	ptr = strdup (encoded);

	/* save the head */
	decoded = ptr;

	/* convert '+' -> ' ' and %2x -> char value */
	while (*ptr)
	{
		switch (*ptr)
		{
		 case '+':
			*ptr = ' ';
			break;
		 case '%':
			if (isxdigit (ptr[1]) && isxdigit (ptr[2]))
			{
				int oct_val;

				oct_val =  oct_value_from_hex (ptr[1]) * 16;
				oct_val += oct_value_from_hex (ptr[2]);

				*ptr = (char) oct_val;

				string_move (ptr + 1, ptr + 3);
			}
			break;
		 default:
			break;
		}

		ptr++;
	}

	return decoded;
}

static char *url_encode_char (char *stream, unsigned char c)
{
	*stream++ = '%';

	sprintf (stream, "%02x", (unsigned int) c);

	return stream + 2;
}

// caller frees returned string
char *fst_utils_url_encode (char *decoded)
{
	char *encoded, *ptr;

	if (!decoded)
		return NULL;

	/* allocate a large enough buffer for all cases */
	encoded = ptr = malloc ((strlen (decoded) * 3) + 1);

	while (*decoded)
	{
		/* we can rule out non-printable and whitespace characters */
		if (!isprint (*decoded) || isspace (*decoded))
			ptr = url_encode_char (ptr, *decoded);
		else
		{
			/* check for anything special */
			switch (*decoded)
			{
			 case '?':
			 case '@':
			 case '+':
			 case '%':
			 case '&':
			 case ':':
			 case '=':
			 case '(':
			 case ')':
			 case '[':
			 case ']':
			 case '\"':
			 case '\'':
				ptr = url_encode_char (ptr, *decoded);
				break;
			 default: /* regular character, just copy */
				*ptr++ = *decoded;
				break;
			}
		}

		decoded++;
	}

	*ptr = 0;

	return encoded;
}

/*****************************************************************************/

// caller frees returned string
char *fst_utils_base64_encode (const unsigned char *data, int src_len)
{
	static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	unsigned char *dst, *out;

	if(!data)
		return NULL;

	if((out = dst = malloc((src_len + 4) * 2)) == NULL)
		return NULL;

	for (; src_len > 2; src_len-=3, dst+=4, data+=3)
	{
		dst[0] = base64[data[0] >> 2];
		dst[1] = base64[((data[0] & 0x03) << 4) + (data[1] >> 4)];
		dst[2] = base64[((data[1] & 0x0f) << 2) + (data[2] >> 6)];
		dst[3] = base64[data[2] & 0x3f];
	}

	dst[0] = '\0';

	if (src_len == 1)
	{
		dst[0] = base64[data[0] >> 2];
		dst[1] = base64[((data[0] & 0x03) << 4)];
		dst[2] = '=';
		dst[3] = '=';
		dst[4] = '\0';
	}

	if (src_len == 2)
	{
		dst[0] = base64[data[0] >> 2];
		dst[1] = base64[((data[0] & 0x03) << 4) + (data[1] >> 4)];
		dst[2] = base64[((data[1] & 0x0f) << 2)];
		dst[3] = '=';
		dst[4] = '\0';
	}

	return out;
}

// caller frees returned string
unsigned char *fst_utils_base64_decode (const char *data, int *dst_len)
{
	static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	unsigned char *dst, *out;
	char *p;
	int i;
	unsigned char in[4];

	if(!data)
		return NULL;

	if((out = dst = malloc(strlen(data))) == NULL)
		return NULL;

	for (i=0, *dst_len=0; *data; data++)
	{
		if((p = strchr(base64, *data)) == NULL)
			continue;

		in[i++] = (unsigned char)(p - base64);

		if (i == 4)
		{
			dst[0] = (in[0] << 2) | ((in[1] & 0x30) >> 4);
			dst[1] = ((in[1] & 0x0F) << 4) | ((in[2] & 0x3C) >> 2);
			dst[2] = ((in[2] & 0x03) << 6) | (in[3] & 0x3F);
			dst += 3;
			*dst_len += 3;
			i = 0;
		}
	}

	if (i >= 2)
	{
		dst[0] = (in[0] << 2) | ((in[1] & 0x30) >> 4);
		(*dst_len)++;
	}

	if (i == 3)
	{
		dst[1] = ((in[1] & 0x0F) << 4) | ((in[2] & 0x3C) >> 2);
		(*dst_len)++;
	}

	return out;
}

/*****************************************************************************/

// returns TRUE if ip in reserved private space, ip is big endian
int fst_utils_ip_private (unsigned int ip)
{
	if (((ip & 0x000000ff) == 0x0000007f) || /* 127.0.0.0 */
	    ((ip & 0x0000ffff) == 0x0000a8c0) || /* 192.168.0.0 */
	    ((ip & 0x00000fff) == 0x000010ac) || /* 172.16-31.0.0 */
	    ((ip & 0x000000ff) == 0x0000000a))   /* 10.0.0.0 */
	{
		return TRUE;
	}

	return FALSE;
}

/*****************************************************************************/

