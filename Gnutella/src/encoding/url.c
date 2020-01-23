/*
 * $Id: url.c,v 1.1 2004/03/24 06:34:36 hipnod Exp $
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

#include "gt_gnutella.h"
#include "encoding/url.h"

#include <ctype.h>

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

char *gt_url_decode (const char *encoded)
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

static char *gt_url_encode_char (char *stream, unsigned char c)
{
	const char hex_alpha[] = "0123456789abcdef";

	*stream++ = '%';

	*stream++ = hex_alpha[(c & 0xf0) >> 4];
	*stream++ = hex_alpha[(c & 0x0f)];

	return stream;
}

/*
 * This is a bit overzealous about what to encode..hopefully that's ok.  This
 * escapes path components ('/').
 */
static BOOL is_safe_char (unsigned char c)
{
	if (c >= 'A' && c <= 'Z')
		return TRUE;

	if (c >= 'a' && c <= 'z')
		return TRUE;

	if (c >= '0' && c <= '9')
		return TRUE;

	switch (c)
	{
	 case '-':
	 case '.':
	 case '_':
		return TRUE;
	 default:
		return FALSE;
	}

	return FALSE;
}

char *gt_url_encode (const char *decoded)
{
	char         *encoded, *ptr;
	unsigned char chr;

	if (!decoded)
		return NULL;

	/* allocate a large enough buffer for all cases */
	encoded = ptr = malloc ((strlen (decoded) * 3) + 1);

	while ((chr = *decoded) != 0)
	{
		if (is_safe_char (chr))
			*ptr++ = chr;
		else
			ptr = gt_url_encode_char (ptr, chr);

		decoded++;
	}

	*ptr = 0;

	return encoded;
}

/*****************************************************************************/

#if 0
int main (int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++)
	{
		char *enc, *dec;

		enc = gt_url_encode (argv[i]);
		dec = gt_url_decode (enc);

		printf ("%s\n%s\n%s\n", argv[i], enc, dec);

		assert (strcmp (argv[i], dec) == 0);
	}

	return 0;
}
#endif
