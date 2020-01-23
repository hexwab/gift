/*
 * memory.c
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

#include "memory.h"

/*****************************************************************************/

void *gift_malloc (size_t size)
{
	if (!size)
		return NULL;

	return malloc (size);
}

void *gift_calloc (size_t nmemb, size_t size)
{
	if (!nmemb || !size)
		return NULL;

	return calloc (nmemb, size);
}

void gift_free (void *ptr)
{
	/* ANSI C89 says it's ok to free NULL, so by god, we'll let free NULL
	 * happen! */
	free (ptr);
}

void *gift_realloc (void *ptr, size_t size)
{
	if (!ptr)
		return gift_malloc (size);

	if (!size)
	{
		gift_free (ptr);
		return NULL;
	}

	return realloc (ptr, size);
}
