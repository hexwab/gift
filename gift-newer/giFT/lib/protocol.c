/*
 * protocol.c
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

#ifndef USE_DLOPEN
int OpenFT_init (Protocol *p);
#endif /* !USE_DLOPEN */

#include "event.h"

/*****************************************************************************/

static List *protocols = NULL;

/*****************************************************************************/

Protocol *protocol_new ()
{
	Protocol *p;

	if (!(p = malloc (sizeof (Protocol))))
	{
		GIFT_ERROR (("out of memory"));
		return NULL;
	}

	memset (p, 0, sizeof (Protocol));

	return p;
}

void protocol_destroy (Protocol *p)
{
	free (p->name);

	dataset_clear (p->support);

	free (p);
}

/*****************************************************************************/

List *protocol_list ()
{
	return protocols;
}

Protocol *protocol_find (char *name)
{
	List *temp;

	if (!name)
		return NULL;

	for (temp = protocols; temp; temp = temp->next)
	{
		Protocol *p = temp->data;

		if (!strcmp (p->name, name))
			return p;
	}

	return NULL;
}

/*****************************************************************************/

void protocol_add (Protocol *p)
{
	protocols = list_append (protocols, p);
}

void protocol_remove (Protocol *p)
{
	protocols = list_remove (protocols, p);
}
