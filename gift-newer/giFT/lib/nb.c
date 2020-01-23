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

#define INITIAL_BUFFER 1024

List *nb_rd = NULL;

NBRead *nb_active (int type)
{
	List *ptr;
	NBRead *nb;

	for (ptr = nb_rd; ptr; ptr = ptr->next)
	{
		nb = ptr->data;
		if (!nb)
			continue;

		if (nb->nb_type == type)
			return nb;
	}

	/* no active read found, let's create one */
	nb = malloc (sizeof (NBRead));
	memset (nb, 0, sizeof (NBRead));

	nb->nb_type = type;

	nb_rd = list_prepend (nb_rd, nb);

	return nb;
}

void nb_terminate (NBRead *nb)
{
	if (!nb)
		return;

	/* using nb_add to prevent buffer overflow, kind of a hack */
	nb_add (nb, 0);
	nb->len--;

	nb->term = 1;
}

void nb_add (NBRead *nb, char c)
{
	/* if the last packet was terminated, we need to reset the data to
	 * be filled again */
	if (nb->term)
	{
		free (nb->data);

		nb->len = nb->datalen = nb->term = 0;

		nb->data = NULL;
	}

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

int nb_read (NBRead *nb, int source, size_t count, char *term)
{
	char buf[RW_BUFFER], *ptr;
	int n, len, tlen, term_len = 0, ret, max_buf_len;
	unsigned int flags;

	if (nb->term)
		nb->len = nb->datalen = 0;

#if 0
	max_buf_len = (adjust_buf < 1.0) ? (int)(adjust_buf*((sizeof(buf) - 1))) :
		(sizeof(buf) - 1);

	max_buf_len = (max_buf_len == 0) ? MIN_BUFFER : max_buf_len;
#endif

	max_buf_len = RW_BUFFER - 1;

	/* if we specify a count, force that amount of bytes, else, we'll
	 * read until we meet term */
	len = count ? (count - nb->len) : max_buf_len;
	flags = count ? 0 : MSG_PEEK;

	if (count == 0)
		term_len = strlen (term);

	if (len > max_buf_len)
	  len = max_buf_len;

	tlen = 0;

	n = recv (source, buf, len, flags);

	if (n <= 0)
		return n;

	/* save return value */
	ret = n;

	if (count == 0)
		buf[n] = 0;

	for (ptr = buf; n > 0; ptr++, n--)
	{
		if (!count && !strncmp (ptr, term, term_len))
		{
			nb_terminate (nb);

			/* we have found the term char(s), push tlen bytes off the queue */
			return recv (source, buf, tlen + term_len, 0);
		}

		nb_add (nb, *ptr);
		tlen++;
	}

	/* we didn't find it in this packet, push it all off the queue
	 * and return */
	if (!count)
		ret = recv (source, buf, len, 0);

	if (count && nb->len == count)
		nb_terminate (nb);

	return ret;
}

void nb_free (NBRead *nb)
{
	if (!nb)
		return;

	free (nb->data);
	free (nb);
}

void nb_finish (NBRead *nb)
{
	nb_rd = list_remove (nb_rd, nb);

	nb_free (nb);
}

void nb_destroy (int type)
{
	List *ptr, *prev;
	NBRead *nb;

	prev = NULL;

	if (!nb_rd)
		return;

	for (ptr = nb_rd; ptr; ptr = ptr->next)
	{
		nb = ptr->data;
		if (!nb)
			continue;

		if (nb->type == type)
		{
			nb_free (nb);
			nb_rd = list_remove (nb_rd, ptr->data);
			ptr = prev;
		}

		prev = ptr;

		if (!ptr)
			break;
	}
}

void nb_destroy_all ()
{
	List *ptr;
	NBRead *nb;

	if (!nb_rd)
		return;

	for (ptr = nb_rd; ptr; ptr = ptr->next)
	{
		nb = ptr->data;
		if (!nb)
			continue;

		nb_free (nb);
	}

	list_free (nb_rd);

	nb_rd = NULL;
}
