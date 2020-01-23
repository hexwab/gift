/*
 * $Id: push_proxy.c,v 1.2 2004/06/02 07:13:02 hipnod Exp $
 *
 * Copyright (C) 2004 giFT project (gift.sourceforge.net)
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

#include "gt_node.h"
#include "gt_packet.h"     /* htovs() */

#include "transfer/push_proxy.h"

/*****************************************************************************/

/*
 * All this GGEP stuff will move somewhere else soon.  Just haven't decided
 * where to put it.
 */

/*****************************************************************************/

#define PROXY_DEBUG          gt_config_get_int("push_proxy/debug=0")

/*****************************************************************************/

#define GGEP_HDR_LEN         (1)    /* 0xc3 */
#define GGEP_EXT_MAX_LEN     (63)   /* use only a single len chunk for now */

enum ggep_length_flags
{
	GGEP_LEN_NOT_LAST      = 0x80,  /* not the last chunk */
	GGEP_LEN_LAST          = 0x40,  /* last length chunk flag */
};

enum ggep_extension_flags
{
	GGEP_EXTF_LAST         = 0x80,  /* last extension */
	GGEP_EXTF_COBS_ENCODED = 0x40,  /* encoded with COBS-encoding */
	GGEP_EXTF_COMPRESSED   = 0x20,  /* compressed */
	GGEP_EXTF_RESERVED     = 0x10,  /* reserved */

	/* lower 4 bits is identifier length */
};

/*****************************************************************************/

struct proxy_addr
{
	in_addr_t  ipv4;
	in_port_t  port;
};

typedef struct ggep
{
	uint8_t   *block;
	size_t     block_len;
	size_t     offset;
	size_t     last_ext_offset;
	BOOL       error;
} ggep_t;

/*****************************************************************************/

static ggep_t     proxy_block;
static Dataset   *proxies;

/*****************************************************************************/

static BOOL ggep_grow (ggep_t *ggep, size_t sz)
{
	uint8_t *new_block;
	size_t   new_size;

	new_size = ggep->block_len + sz;
	if (!(new_block = realloc (ggep->block, new_size)))
		return FALSE;

	ggep->block     = new_block;
	ggep->block_len = new_size;

	return TRUE;
}

static BOOL ggep_init (ggep_t *ggep)
{
	ggep->block_len       = 1;
	ggep->offset          = 1;
	ggep->last_ext_offset = 0;
	ggep->error           = FALSE;

	if (!(ggep->block = malloc (1)))
		return FALSE;

	/* append magic byte */
	ggep->block[0] = 0xc3;

	return TRUE;
}

static void ggep_append (ggep_t *ggep, const void *data, size_t data_size)
{
	if (ggep_grow (ggep, data_size) == FALSE)
	{
		ggep->error = TRUE;
		return;
	}

	assert (ggep->offset + data_size <= ggep->block_len);
	memcpy (&ggep->block[ggep->offset], data, data_size);
	ggep->offset += data_size;
}

/* TODO: this should use a writev()-like interface */
static BOOL ggep_append_extension (ggep_t *ggep, const char *id,
                                   const uint8_t *data, size_t data_len)
{
	uint8_t id_len;
	uint8_t ext_flags;
	uint8_t ext_data_len;

	id_len = strlen (id) & 0x0f;

	/* disable Encoding, Compression, LastExtension bits, len in low 4 bits */
	ext_flags = id_len;

	/* track position of last extension for setting LastExtension bit */
	ggep->last_ext_offset = ggep->offset;

	/* extension flag header */
	ggep_append (ggep, &ext_flags, 1);

	/* extension identifier */
	ggep_append (ggep, id, id_len);

	assert (data_len <= GGEP_EXT_MAX_LEN);
	ext_data_len = data_len | GGEP_LEN_LAST; /* add last length chunk flag */

	/* the extension length */
	ggep_append (ggep, &ext_data_len, 1);

	/* the extension data */
	ggep_append (ggep, data, data_len);

	if (ggep->error)
		return FALSE;

	return TRUE;
}

static BOOL ggep_seal (ggep_t *ggep)
{
	if (ggep->last_ext_offset == 0)
		return FALSE;

	/* set the LastExtension bit on the last extension */
	ggep->block[ggep->last_ext_offset] |= GGEP_EXTF_LAST;
	return TRUE;
}

static void ggep_finish (ggep_t *ggep)
{
	free (ggep->block);
}

/*****************************************************************************/

static void ds_add_proxy (ds_data_t *key, ds_data_t *value, void **cmp)
{
	uint8_t           *push_ext     = cmp[0];
	size_t            *push_ext_len = cmp[1];
	in_port_t          port;
	struct proxy_addr *proxy = value->data;

	port = htovs (proxy->port);

	if (*push_ext_len + 6 >= GGEP_EXT_MAX_LEN)
		return;

	/* build the PUSH extension */
	memcpy (&push_ext[*push_ext_len], &proxy->ipv4, 4); *push_ext_len += 4;
	memcpy (&push_ext[*push_ext_len], &port, 2);        *push_ext_len += 2;
}

static void update_block (ggep_t *ggep)
{
	uint8_t push_ext[GGEP_EXT_MAX_LEN]; /* ugh */
	size_t  push_ext_len;               /* double ugh */
	void   *cmp[2];

	ggep_finish (ggep);

	if (ggep_init (ggep) == FALSE)
		return;

	cmp[0] = push_ext;
	cmp[1] = &push_ext_len;

	push_ext_len = 0;
	dataset_foreach (proxies, DS_FOREACH(ds_add_proxy), cmp);
	assert (push_ext_len <= GGEP_EXT_MAX_LEN);

	if (ggep_append_extension (ggep, "PUSH", push_ext, push_ext_len) == FALSE)
		return;

	ggep_seal (ggep);
}

/*****************************************************************************/

static void push_proxy_change (GtNode *node, in_addr_t ipv4,
                               in_port_t port, BOOL add)
{
	struct proxy_addr  addr;
	struct proxy_addr *stored;

	addr.ipv4 = ipv4;
	addr.port = port;

	stored = dataset_lookup (proxies, &node, sizeof(node));
	if (PROXY_DEBUG)
	{
		if (add && !stored)
			GT->DBGFN (GT, "adding push proxy %s:%hu", net_ip_str (ipv4), port);
		else if (!add && stored)
			GT->DBGFN (GT, "rming push proxy %s:%hu", net_ip_str (ipv4), port);
	}

	if (add)
		dataset_insert (&proxies, &node, sizeof(node), &addr, sizeof(addr));
	else
		dataset_remove (proxies, &node, sizeof(node));

	update_block (&proxy_block);
}

void gt_push_proxy_add (GtNode *node, in_addr_t ipv4, in_port_t port)
{
	assert (node->push_proxy_ip == 0);
	assert (node->push_proxy_port == 0);

	push_proxy_change (node, ipv4, port, TRUE);
	node->push_proxy_ip   = ipv4;
	node->push_proxy_port = port;
}

/*
 * This must be called if the port changes, or if the proxy disconnects.
 */
void gt_push_proxy_del (GtNode *node)
{
	push_proxy_change (node, node->push_proxy_ip,
	                   node->push_proxy_port, FALSE);
	node->push_proxy_ip   = 0;
	node->push_proxy_port = 0;
}

BOOL gt_push_proxy_get_ggep_block (uint8_t **block, size_t *block_len)
{
	if (dataset_length (proxies) == 0)
		return FALSE;

	*block     = proxy_block.block;
	*block_len = proxy_block.block_len;

	return TRUE;
}

/*****************************************************************************/

void gt_push_proxy_init (void)
{
	ggep_init (&proxy_block);
}

void gt_push_proxy_cleanup (void)
{
	dataset_clear (proxies);
	proxies = NULL;

	ggep_finish (&proxy_block);
}
