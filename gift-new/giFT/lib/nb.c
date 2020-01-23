/*
 * nb.c
 *
 * non blocking implementations of various socket functions
 * another borrowed subsystem from Gnapster :)
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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

#include "gift.h"

#include "network.h"
#include "nb.h"
#include "list.h"

/*****************************************************************************/

#define INITIAL_BUFFER 512

static HashTable *nb_buffers = NULL;

/*****************************************************************************/

#if 0
static int match_fd (NBRead *nb, void *fd)
{
	if (nb->fd != P_INT (fd))
		return -1;

	return 0;
}
#endif

NBRead *nb_active (int fd)
{
	NBRead *nb;

	if (!(nb = hash_table_lookup (nb_buffers, fd)))
	{
		/* not found, create one */
		nb = malloc (sizeof (NBRead));
		memset (nb, 0, sizeof (NBRead));

		nb->fd = fd;

		if (!nb_buffers)
			nb_buffers = hash_table_new ();

		hash_table_insert (nb_buffers, fd, nb);
	}

	return nb;
}

void nb_finish (NBRead *nb)
{
	if (!nb)
		return;

#ifdef DEBUG
	/* check if they supplied something bogus */
	assert (hash_table_lookup (nb_buffers, nb->fd));
#endif /* DEBUG */

	hash_table_remove (nb_buffers, nb->fd);

	free (nb->data);
	free (nb);
}

void nb_remove (int fd)
{
	NBRead *nb;

	if (!(nb = hash_table_lookup (nb_buffers, fd)))
		return;

	nb_finish (nb);
}

static int remove_one (unsigned long key, NBRead *nb, void *udata)
{
	nb_finish (nb);
	return TRUE;
}

/* mass cleanup */
void nb_remove_all ()
{
	hash_table_foreach_remove (nb_buffers, (HashFunc) remove_one, NULL);
	hash_table_destroy (nb_buffers);
	nb_buffers = NULL;

#if 0
	while (nb_buffers)
		nb_finish (nb_buffers->data);
#endif
}

/*****************************************************************************/

static void nb_unterminate (NBRead *nb)
{
	if (!nb->term)
		return;

	nb->term = FALSE;
	nb->len  = 0;

	if (nb->datalen)
		*nb->data = 0;
	else
	{
		free (nb->data);
		nb->data = NULL;
	}
}

static void nb_add (NBRead *nb, char c)
{
	/* if the last packet was terminated, we need to reset the data to
	 * be filled again */
	nb_unterminate (nb);

	/* check to make sure we need more mem */
	if (nb->len >= nb->datalen)
	{
		/* allocate the initial size */
		if (!nb->datalen)
			nb->datalen = INITIAL_BUFFER;
		else
			nb->datalen *= 2;

		nb->data = realloc (nb->data, nb->datalen);
	}

	nb->data[nb->len++] = c;
}

static void nb_terminate (NBRead *nb)
{
	if (!nb)
		return;

	/* using nb_add to prevent buffer overflow, but we must actually rewind
	 * the len so that it isn't counted as valid data */
	nb_add (nb, 0);
	nb->len--;

	nb->term = TRUE;
}

/*****************************************************************************/

/* see wrappers below
 * NOTE: this function can be dangerous if you use it wrong!
 */
int nb_read (NBRead *nb, size_t count, char *term)
{
	char         buf[RW_BUFFER], *buf_ptr;
	int          n, i;
	size_t       term_len = 0;
	size_t       len      = 0;
	unsigned int flags    = 0;

	/* so that we dont make possibly inaccurate guesses about how much
	 * data is left to be read */
	nb_unterminate (nb);

	if (count)
		len = count - nb->len;
	else
	{
		/* peek as much data as possible to look for the sentinel */
		len   = sizeof (buf) - 1;
		flags = MSG_PEEK;

		/* precalculate the length of the sentinel */
		term_len = strlen (term);
	}

	if (len > sizeof (buf) - 1)
		len = sizeof (buf) - 1;

	if ((n = recv (nb->fd, buf, len, flags)) <= 0)
		return n;

	/* process the data from the socket */
	for (buf_ptr = buf, i = 0; i < n; i++, buf_ptr++)
	{
		/* check for the sentinel */
		if (term && !strncmp (buf_ptr, term, term_len))
		{
			nb_terminate (nb);

			/* count the sentinel as data read */
			i += term_len;

			break;
		}

		nb_add (nb, *buf_ptr);
	}

	if (term)
	{
		/* push off all the data processed from the socket queue */
		return recv (nb->fd, buf, i, 0);
	}

	if (nb->len == count)
		nb_terminate (nb);

	return n;
}

/*****************************************************************************/
/* NB_READ WRAPPERS */

/* read a set length */
int nb_read_len (NBRead *nb, int len)
{
	return nb_read (nb, len, NULL);
}

/* read up to the supplied sentinel */
int nb_read_term (NBRead *nb, char *term)
{
	return nb_read (nb, 0, term);
}
