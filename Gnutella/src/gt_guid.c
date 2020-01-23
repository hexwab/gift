/*
 * $Id: gt_guid.c,v 1.18 2006/01/28 16:57:56 mkern Exp $
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
#include "sha1.h"

/*****************************************************************************/

/* global guid identifying this client */
gt_guid_t *GT_SELF_GUID;

/*****************************************************************************/

/* seed for generating unique numbers */
static unsigned int seed = 0;

/* map binary numbers to hexadecimal */
static char bin_to_hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                             'a', 'b', 'c', 'd', 'e', 'f' };

/* guid filled with all zeroes */
static gt_guid_t zero_guid[GT_GUID_LEN] = { 0 };

/*****************************************************************************/

/* TODO: use /dev/urandom when available */
static unsigned int rng_seed (void)
{
	sha1_state_t   sha1;
	struct timeval tv;
	unsigned int   seed;
	int            i;
	unsigned char  hash[SHA1_BINSIZE];

	gt_sha1_init (&sha1);
	platform_gettimeofday (&tv, NULL);

	gt_sha1_append (&sha1, &tv.tv_usec, sizeof (tv.tv_usec));
	gt_sha1_append (&sha1, &tv.tv_sec, sizeof (tv.tv_sec));

#ifdef HAVE_GETPID
	{
		pid_t pid = getpid();
		gt_sha1_append (&sha1, &pid, sizeof (pid));
	}
#endif

#ifdef WIN32
	{
		DWORD process_id = GetCurrentProcessId();
		gt_sha1_append (&sha1, &process_id, sizeof (process_id));
	}
#endif

#ifdef HAVE_GETPPID
	{
		pid_t ppid = getppid();
		gt_sha1_append (&sha1, &ppid, sizeof (ppid));
	}
#endif /* WIN32 */

	memset (hash, 0, sizeof (hash));
	gt_sha1_finish (&sha1, hash);

	seed = 0;
	i    = 0;

	/* crush the hash into an unsigned int */
	while (i < SHA1_BINSIZE)
	{
		unsigned int t;
		size_t       len;

		t = 0;
		len = MIN (sizeof (unsigned int), SHA1_BINSIZE - i);

		memcpy (&t, &hash[i], len);

		seed ^= t;
		i += len;
	}

	return seed;
}

void gt_guid_init (gt_guid_t *guid)
{
	int i;

	if (!seed)
	{
		seed = rng_seed();
		srand (seed);
	}

	for (i = GT_GUID_LEN - 1; i >= 0; i--)
		guid[i] = 256.0 * rand() / (RAND_MAX + 1.0);

	/* mark this GUID as coming from a "new" client */
	guid[8]  = 0xff;
	guid[15] = 0x01;
}

gt_guid_t *gt_guid_new (void)
{
	gt_guid_t *guid;

	if (!(guid = malloc (GT_GUID_LEN)))
		return NULL;

	gt_guid_init (guid);

	return guid;
}

int gt_guid_cmp (const gt_guid_t *a, const gt_guid_t *b)
{
	if (!a)
		return (b == NULL ? 0 : -1);
	else if (!b)
		return (a == NULL ? 0 : +1);

	return memcmp (a, b, GT_GUID_LEN);
}

char *gt_guid_str (const gt_guid_t *guid)
{
	static char    buf[128];
	unsigned char  c;
	int            pos;
	int            len;

	if (!guid)
		return NULL;

	pos = 0;
	len = GT_GUID_LEN;

	while (len-- > 0)
	{
		c = *guid++;

		buf[pos++] = bin_to_hex[(c & 0xf0) >> 4];
		buf[pos++] = bin_to_hex[(c & 0x0f)];
	}

	buf[pos] = 0;

	return buf;
}

gt_guid_t *gt_guid_dup (const gt_guid_t *guid)
{
	gt_guid_t *new_guid;

	if (!(new_guid = malloc (GT_GUID_LEN)))
		return NULL;

	memcpy (new_guid, guid, GT_GUID_LEN);

	return new_guid;
}

static unsigned char hex_char_to_bin (char x)
{
	if (x >= '0' && x <= '9')
		return (x - '0');

	x = toupper (x);

	return ((x - 'A') + 10);
}

static int hex_to_bin (const char *hex, unsigned char *bin, int len)
{
	unsigned char value;

	while (isxdigit (hex[0]) && isxdigit (hex[1]) && len-- > 0)
	{
		value  = (hex_char_to_bin (*hex++) << 4) & 0xf0;
		value |= (hex_char_to_bin (*hex++)       & 0x0f);
		*bin++ = value;
	}

   return (len <= 0) ? TRUE : FALSE;
}

gt_guid_t *gt_guid_bin (const char *guid_ascii)
{
	gt_guid_t *guid;

	if (!guid_ascii)
		return NULL;

	if (!(guid = malloc (GT_GUID_LEN)))
		return NULL;

	if (!hex_to_bin (guid_ascii, guid, GT_GUID_LEN))
	{
		free (guid);
		return NULL;
	}

	return guid;
}

/*****************************************************************************/

BOOL gt_guid_is_empty (const gt_guid_t *guid)
{
	if (!guid)
		return TRUE;

	return memcmp (guid, zero_guid, GT_GUID_LEN) == 0;
}

/*****************************************************************************/

/*
 * Load the client GUID for this node.
 */
static gt_guid_t *get_client_id (char *conf_path)
{
	FILE      *f;
	gt_guid_t *client_id = NULL;
	char      *buf       = NULL;

	/*
	 * There are people who distribute giFT packages which include the
	 * developer's client-id file. These packages (mainly KCeasy derivatives,
	 * I think) are widely enough distributed that other gnutella developers
	 * have noticed the problem and notified me. To prevent this problem in
	 * future versions I have forced randomization of the GUID on each startup
	 * below. --mkern
	 */
#if 0
	if ((f = fopen (gift_conf_path (conf_path), "r")))
	{
		while (file_read_line (f, &buf))
		{
			char *id;
			char *line;

			free (client_id);
			client_id = NULL;

			line = buf;
			id = string_sep_set (&line, "\r\n");

			if (string_isempty (id))
				continue;

			client_id = gt_guid_bin (id);
		}

		fclose (f);
	}
#endif

	/* TODO: regenerate when client guid is pretty old */
	if (client_id != NULL)
		return client_id;

	/*
	 * Create a new client identifier
	 */
	client_id = gt_guid_new ();
	assert (client_id != NULL);

	/* store the id in ~/.giFT/Gnutella/clientid */
	if (!(f = fopen (gift_conf_path (conf_path), "w")))
	{
		GIFT_ERROR (("clientid storage file: %s", GIFT_STRERROR ()));
		return client_id;
	}

	fprintf (f, "%s\n", gt_guid_str (client_id));
	fclose (f);

	return client_id;
}

void gt_guid_self_init (void)
{
	GT_SELF_GUID = get_client_id ("Gnutella/client-id");

	/* remove the old clientid file which used an inferior random number
	 * generator */
	remove (gift_conf_path ("Gnutella/clientid"));
}

void gt_guid_self_cleanup (void)
{
	free (GT_SELF_GUID);
	GT_SELF_GUID = NULL;
}
