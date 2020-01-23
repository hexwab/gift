/*
 * $Id: as_util.c,v 1.3 2005/09/15 21:13:53 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* returns TRUE if ip is routable on the internet */
as_bool net_ip_routable (in_addr_t ip)
{
	ip = ntohl (ip);

	/* TODO: filter multicast and broadcast */
	if (((ip & 0xff000000) == 0x7f000000) || /* 127.0.0.0 */
	    ((ip & 0xffff0000) == 0xc0a80000) || /* 192.168.0.0 */
	    ((ip & 0xfff00000) == 0xac100000) || /* 172.16-31.0.0 */
	    ((ip & 0xff000000) == 0x0a000000) || /* 10.0.0.0 */
	    ((ip & 0xE0000000) == 0xE0000000) || /* Classes D,E,F */
		(ip == 0) ||
		(ip == INADDR_NONE)) /* invalid ip */
	{
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

/* These functions are not in libgift. We duplicate code here so we can use
 * it easily with libgift later. 
 */

/* Insert link after prev. Returns prev. */
List *list_insert_link (List *prev, List *link)
{
	if (!prev || !link)
		return prev;

	link->prev = prev;
	link->next = prev->next;
	prev->next = link;
	if (link->next)
		link->next->prev = link;

	return prev;
}


/* Remove link from list but do not free it. */
List *list_unlink_link (List *head, List *link)
{
	if (!head)
		return NULL;

	if (!link)
		return head;

	if (link == head)
		head = link->next;

	if (link->prev)
		link->prev->next = link->next;

	if (link->next)
		link->next->prev = link->prev;

	link->prev = NULL;
	link->next = NULL;

	return head;	
}

/* insert new_link between prev and next, returns link */
static List *insert_link (List *prev, List *next, List *new_link)
{
	if (prev == NULL && next == NULL)
	{
		new_link->prev = NULL;
		new_link->next = NULL;
	}
	else if (prev == NULL)
	{
		assert (next->prev == NULL);

		new_link->prev = NULL;
		new_link->next = next;
		next->prev = new_link;
	}
	else if (next == NULL)
	{
		assert (prev->next == NULL);

		new_link->prev = prev;
		new_link->next = NULL;
		prev->next = new_link;
	}
	else
	{
		assert (prev->next == next);
		assert (next->prev == prev);

		new_link->prev = prev;
		new_link->next = next;
		prev->next = new_link;
		next->prev = new_link;
	}

	return new_link;
}

/* Same as list_insert_sorted but uses supplied link instead of creating a
 * new one.
 */
List *list_insert_link_sorted (List *head, CompareFunc func, List *new_link)
{
	List *link;

	assert (func);
	assert (new_link);

	if (!head)
		return insert_link (NULL, NULL, new_link);

	if (func (head->data, new_link->data) >= 0)
		return insert_link (NULL, head, new_link);

	for (link = head; link->next; link = link->next)
	{
		if (func (link->next->data, new_link->data) >= 0)
		{
			insert_link (link, link->next, new_link);
			return head;
		}
	}

	/* insert as last element */
	insert_link (link, NULL, new_link);

	return head;
}

/* Returns true if list is linked correctly and all data fileds are
 * non-NULL. If halt is true we assert at the point of error.
 */
as_bool list_verify_integrity (List *head, as_bool halt)
{
	List *curr = head, *prev = NULL;

	while (curr)
	{
		/* Must have data. */
		if (!curr->data)
		{
			if (halt)
				assert (curr->data);
			return FALSE;
		}

		/* This link must point to previous one. */
		if (curr->prev != prev)
		{
			if (halt)
				assert (curr->prev == prev);
			return FALSE;
		}

		/* Previous link must point to this one of present. */
		if (prev && prev->next != curr)
		{
			if (halt)
				assert (prev->next == curr);
			return FALSE;
		}

		prev = curr;
		curr = curr->next;
	}

	return TRUE;
}

/*****************************************************************************/
