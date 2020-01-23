/*
 * $Id: gt_ban.c,v 1.4 2003/07/09 09:40:23 hipnod Exp $
 *
 * Copyright (C) 2003 giFT project (gift.sourceforge.net)
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

#include "gt_conf.h"

/*****************************************************************************/

/* mask of bits that determines which bucket a ban goes into */
#define  INDEX_MASK              (0xff000000)

/*****************************************************************************/

typedef struct ban_ipv4
{
	in_addr_t     ipv4;
	uint32_t      netmask;
} ban_ipv4_t;

/*****************************************************************************/

/* ban list organize by the first 8 bits */
static Dataset *ipv4_ban_list;

/*****************************************************************************/

static char *net_mask_str (uint32_t netmask)
{
	static char buf[128];

	snprintf (buf, sizeof (buf) - 1, "%d.%d.%d.%d",
	          (netmask & 0xff000000) >> 24,
	          (netmask & 0x00ff0000) >> 16,
	          (netmask & 0x0000ff00) >> 8,
	          (netmask & 0x000000ff));

	return buf;
}

static uint32_t net_mask_bin (char *netmask)
{
	uint32_t a[4];

	if (!netmask)
		return 0xFFFFffff;

	if (sscanf (netmask, "%d.%d.%d.%d", &a[0], &a[1], &a[2], &a[3]) != 4)
		return 0xFFFFffff;

	return (a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
}

/*****************************************************************************/

/* 
 * Returns 0 if ban 'b' is contained within ban 'a'.
 */
static int find_superset_ban (ban_ipv4_t *a, ban_ipv4_t *b)
{
	uint32_t sig1 = a->ipv4 & a->netmask;
	uint32_t sig2 = b->ipv4 & b->netmask;

	/*
	 * If there are no bits in the significant address portions
	 * that contradict within _a_'s netmask, then ban _b_ is a
	 * subset of _a_.
	 */
	return ((sig2 ^ sig1) & a->netmask);
}

static void log_dup (ban_ipv4_t *orig_ban, ban_ipv4_t *b)
{
	char  *ip1, *ip2;
	char  *n1,  *n2;

	if (!orig_ban || !b)
		return;

	n1  = STRDUP (net_mask_str (b->netmask));
	ip1 = STRDUP (net_ip_str (htonl (b->ipv4)));
	n2  = net_mask_str (orig_ban->netmask);
	ip2 = net_ip_str (htonl (orig_ban->ipv4));

	GT->dbg (GT, "ban %s/%s is a subset of %s/%s", ip1, n1, ip2, n2);

	free (ip1); 
	free (n1);
}

BOOL gt_ban_ipv4_add (in_addr_t address, uint32_t netmask)
{
	uint32_t     prefix;
	List        *list;
	List        *orig;
	ban_ipv4_t  *ban;

	if (!(ban = MALLOC (sizeof (ban_ipv4_t))))
		return FALSE;

	address = ntohl (address);

	ban->ipv4    = address;
	ban->netmask = netmask | INDEX_MASK;  /* ensure we have at least 8 bits */

	prefix = address & INDEX_MASK;

	list = dataset_lookup (ipv4_ban_list, &prefix, sizeof (prefix));

	if ((orig = list_find_custom (list, ban, (ListForeachFunc)find_superset_ban)))
	{
		log_dup (list_nth_data (orig, 0), ban);
		free (ban);
		return TRUE;
	}

	list = list_prepend (list, ban);

	if (!dataset_insert (&ipv4_ban_list, &prefix, sizeof (prefix), list, 0))
		return FALSE;

	GT->dbg (GT, "*!*@%s/%s", net_ip_str (htonl (address)), 
	         net_mask_str (netmask));

	return TRUE;
}

BOOL gt_ban_ipv4_is_banned (in_addr_t address)
{
	uint32_t    prefix;
	List       *list;
	ban_ipv4_t  ban;

	address = ntohl (address);
	prefix  = address & INDEX_MASK;

	if (!(list = dataset_lookup (ipv4_ban_list, &prefix, sizeof (prefix))))
		return FALSE;

	ban.ipv4    = address;
	ban.netmask = 0xFFFFffff;

	if (list_find_custom (list, &ban, (ListForeachFunc)find_superset_ban))
		return TRUE;
		
	return FALSE;
}

/*****************************************************************************/

static BOOL load_hostiles_txt (char *hostiles_filename)
{
	FILE  *f;
	char  *hostiles_path;
	char  *buf = NULL;
	char  *p;

	hostiles_path = gift_conf_path ("%s/%s", GT->name, hostiles_filename);

	if (!(f = fopen (hostiles_path, "r")))
		return FALSE;

	while (file_read_line (f, &buf))
	{
		in_addr_t  ipv4;
		uint32_t   netmask;
		char      *ipv4_str;
		char      *mask_str;

		p = buf;

		/* divide the string into two sides */
		ipv4_str = string_sep (&p, "/");
		mask_str = p;

		if (!ipv4_str)
			continue;

		netmask = net_mask_bin (mask_str);
		ipv4    = net_ip       (ipv4_str);

		if (!ipv4)
			continue;

		gt_ban_ipv4_add (ipv4, netmask);
	}

	fclose (f);
	return TRUE;
}

void gt_ban_init (void)
{
	ipv4_ban_list = dataset_new (DATASET_HASH);

	if (!gt_config_load_file ("Gnutella/hostiles.txt", FALSE, TRUE))
		GT->warn (GT, "couldn't load \"hostiles.txt\"");

	load_hostiles_txt ("hostiles.txt");
#ifndef WIN32
	load_hostiles_txt ("Hostiles.txt"); /* case-sensitivite blah */
#endif
}

static int free_ban (ban_ipv4_t *ban, void *udata)
{
	free (ban);
	return TRUE;
}

static int cleanup_lists (ds_data_t *data, ds_data_t *value, void *udata)
{
	List *list = value->data;
	list_foreach_remove (list, (ListForeachFunc)free_ban, NULL);
	return DS_CONTINUE | DS_REMOVE;
}

void gt_ban_cleanup (void)
{
	dataset_foreach_ex (ipv4_ban_list, DS_FOREACH_EX(cleanup_lists), NULL);
	dataset_clear (ipv4_ban_list);
	ipv4_ban_list = NULL;
}
