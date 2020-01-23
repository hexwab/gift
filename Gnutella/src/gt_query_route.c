/*
 * $Id: gt_query_route.c,v 1.46 2004/04/05 07:56:54 hipnod Exp $
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

#include "gt_query_route.h"
#include "gt_packet.h"
#include "gt_utils.h"

#include "gt_node.h"
#include "gt_node_list.h"

#include <libgift/stopwatch.h>

#include <zlib.h>

/*****************************************************************************/

/*
 * TODO:
 *      - compact table to bit-level representation 
 *      - support incremental updates of table
 *      - cut-off entries when MAX_FILL_RATIO is reached
 */
#define MIN_TABLE_BITS        (16)                          /* 16   bits  */
#define MAX_TABLE_BITS        (21)                          /* 21   bits  */
#define MIN_TABLE_SIZE        (1UL << (MIN_TABLE_BITS - 1)) /* 32k  bytes */
#define MAX_TABLE_SIZE        (1UL << (MAX_TABLE_BITS - 1)) /* 1M   bytes */
#define MIN_TABLE_SLOTS       (MIN_TABLE_SIZE * 2)          /* 64k  slots */
#define MAX_TABLE_SLOTS       (MAX_TABLE_SIZE * 2)          /* 2M   slots */
#define INC_FILL_RATIO        (0.70)                        /* 0.7%       */
#define MAX_FILL_RATIO        (1.00)                        /* 1% (unused)*/

/*
 * magical constant necessary for the query routing hash function
 */
#define MULTIPLIER            0x4F1BBCDC

/*
 * How often to synchronize the QRT with ultrapeers.
 *
 * This is very big because we don't support incremental updating
 * yet.
 */
#define QRT_UPDATE_INTERVAL   (20 * MINUTES)

/*
 * How often we check to build a compressed patch of local shares.
 */
#define QRT_BUILD_INTERVAL    (3 * SECONDS)

/*
 * How long to wait after the first query_route_table_submit() before
 * actually submitting the table.
 */
#define QRT_SUBMIT_DELAY      (1 * MINUTES)

/*
 * Largest hops value in table. It looks like Limewire hardcodes
 * this as 7, and won't understand any other value.
 */
#define INFINITY              7

/*
 * Constants for changing route table.
 */
#define QRT_KEYWORD_ADD       (0x0a)           /* -6 */
#define QRT_KEYWORD_REMOVE    (0x06)           /*  6 */

/*
 * The minimum length of a keyword
 */
#define QRP_MIN_LENGTH        3

/*
 * Maximum patch fragment size to send
 */
#define PATCH_FRAGSIZE        2048

/*
 * Number of bits in the patches we send
 */
#define PATCH_BITS            4

/*
 * Holds a 32-bit index describing each token this node shares.
 */
struct qrp_route_entry
{
	int        ref;                    /* number of references to this index */
	uint32_t   index;                  /* the actual position */
};

struct qrp_route_table
{
	uint8_t   *table;
	size_t     bits;
	size_t     size;
	size_t     slots;
	size_t     present;
	size_t     shared;
};

/*****************************************************************************/

/*
 * The set of indices that are currently marked "present" in the
 * routing table.
 */
static Dataset       *indices;

/*
 * This holds the compressed table as a full QRP patch (incremental
 * updates not supported yet).
 */
static uint8_t *compressed_table;
static size_t   compressed_slots;
static size_t   compressed_size;

/*
 * Keeps track of how many times the compressed table has changed.
 * Used to avoid sending updates when not necessary.
 */
static int            compressed_version;

/*
 * Timer that builds the compressed patch for the table submitted
 * to peers.
 */
static timer_id       build_timer;

/*
 * Whether we have to rebuild the table when shares are done
 * syncing.
 */
static BOOL           table_changed;

/*
 * The full query-routing table in binary form. This will get
 * compressed before transmission.
 */
static struct qrp_route_table *route_table;

/*****************************************************************************/

/* hash function used for query-routing */
uint32_t gt_query_router_hash_str (char *str, size_t bits)
{
	uint32_t      hash;
	unsigned int  i;
	unsigned char c;

	i = hash = 0;

	while ((c = *str++) && !isspace (c))
	{
		hash ^= tolower (c) << (i * 8);

		/* using & instead of %, sorry */
		i = (i+1) & 0x03;
	}

	return (hash * MULTIPLIER) >> (32 - bits);
}

/*****************************************************************************/

static struct qrp_route_table *qrp_route_table_new (size_t bits)
{
	struct qrp_route_table *qrt;

	if (!(qrt = MALLOC (sizeof (struct qrp_route_table))))
		return NULL;

	qrt->bits  = bits;
	qrt->size  = (1UL << (bits - 1));
	qrt->slots = qrt->size * 2;        /* 4-bit entries only */

	if (!(qrt->table = MALLOC (qrt->size)))
	{
		free (qrt);
		return NULL;
	}

	return qrt;
}

static void qrp_route_table_free (struct qrp_route_table *qrt)
{
	if (!qrt)
		return;

	free (qrt->table);
	free (qrt);
}

static void qrp_route_table_insert (struct qrp_route_table *qrt, uint32_t index)
{
	uint8_t   old_entry;
	int       set_lower;
	int       entry;

	if (!qrt)
		return;

	assert (index < qrt->size * 2);

	set_lower = index % 2;
	entry     = index / 2;

	if (set_lower)
	{
		old_entry = qrt->table[entry] & 0x0f;
		qrt->table[entry] = (qrt->table[entry] & 0xf0) |
		                    ((QRT_KEYWORD_ADD) & 0x0f);
	}
	else
	{
		old_entry = (qrt->table[entry] & 0xf0) >> 4;
		qrt->table[entry] = (qrt->table[entry] & 0x0f) |
		                    ((QRT_KEYWORD_ADD << 4) & 0xf0);
	}

	assert (old_entry == 0 || old_entry == QRT_KEYWORD_REMOVE);
#if 0
	GT->dbg (GT, "+%u [%d/%d]", index, entry, set_lower);
#endif

	qrt->present++;
}

#if 0
/* untested */
static void qrp_route_table_remove (struct qrp_route_table *qrt, uint32_t index)
{
	uint8_t   old_entry;
	int       clear_lower;
	int       entry;

	if (!qrt)
		return;

	assert (index < qrt->size * 2);

	clear_lower = index % 2;
	entry       = index / 2;

	/*
	 * This needs to change when doing incremental updating...
	 */

	if (clear_lower)
	{
		old_entry = qrt->table[entry] & 0x0f;
		qrt->table[entry] = (qrt->table[entry] & 0xf0) |
		                    ((QRT_KEYWORD_REMOVE) & 0x0f);
	}
	else
	{
		old_entry = (qrt->table[entry] & 0xf0) >> 4;
		qrt->table[entry] = (qrt->table[entry] & 0x0f) |
		                    ((QRT_KEYWORD_REMOVE << 4) & 0xf0);
	}

	assert (old_entry == (uint8_t) QRT_KEYWORD_ADD);
#if 0
	GT->dbg (GT, "-%u [%d/%d]", index, entry, clear_lower);
#endif

	qrt->present--;
}
#endif

static BOOL qrp_route_table_lookup (struct qrp_route_table *qrt, uint32_t index)
{
	int       check_lower;
	uint32_t  entry;

	if (!qrt)
		return FALSE;

	assert (index < qrt->slots);
	assert (qrt->slots == qrt->size * 2);
	        
	check_lower = index % 2;
	entry       = index / 2;

	if (check_lower)
	{
		if ((qrt->table[entry] & 0x0f) == QRT_KEYWORD_ADD)
			return TRUE;
	}
	else
	{
		if (((qrt->table[entry] & 0xf0) >> 4) == QRT_KEYWORD_ADD) 
			return TRUE;
	}

	return FALSE;
}

static double qrp_route_table_fill_ratio (struct qrp_route_table *qrt)
{
	return (double)qrt->present * 100 / qrt->slots;
}

/*****************************************************************************/

static char *zlib_strerror (int error)
{
	switch (error)
	{
	 case Z_OK:             return "OK";
	 case Z_STREAM_END:     return "End of stream";
	 case Z_NEED_DICT:      return "Decompressing dictionary needed";
	 case Z_STREAM_ERROR:   return "Stream error";
	 case Z_ERRNO:          return "Generic zlib error";
	 case Z_DATA_ERROR:     return "Data error";
	 case Z_MEM_ERROR:      return "Memory error";
	 case Z_BUF_ERROR:      return "Buffer error";
	 case Z_VERSION_ERROR:  return "Incompatible runtime zlib library";
	 default:               break;
	}

	return "Invalid zlib error code";
}

/* TODO: make this use a stream-like interface */
static uint8_t *compress_table (uint8_t *table, size_t in_size, size_t *out_size)
{
	z_streamp   out;
	int         ret;
	uint8_t    *out_buf;
	int         free_size;

	*out_size = 0;

	if (!(out = MALLOC (sizeof(*out))))
		return NULL;

	out->zalloc = Z_NULL;
	out->zfree  = Z_NULL;
	out->opaque = Z_NULL;

	if ((ret = deflateInit (out, Z_DEFAULT_COMPRESSION)) != Z_OK)
	{
		GT->DBGFN (GT, "deflateInit error: %s", zlib_strerror (ret));
		free (out);
		return NULL;
	}

	/* allocate initial buffer */
	free_size = in_size + in_size / 100;

	if (!(out_buf = malloc (free_size)))
	{
		free (out_buf);
		deflateEnd (out);
		free (out);
		return NULL;
	}

	out->next_in   = table;
	out->avail_in  = in_size;
	out->next_out  = out_buf;
	out->avail_out = free_size;

	if ((ret = deflate (out, Z_FINISH)) != Z_STREAM_END)
	{
		GT->DBGFN (GT, "compression error: %s", zlib_strerror (ret));
		free (out_buf);
		deflateEnd (out);
		free (out);
		return NULL;
	}

	/* 
	 * This could theoretically fail I guess. If it does, we shouldn't keep
	 * the table at least.
	 */
	assert (out->avail_in == 0);

	*out_size = free_size - out->avail_out;

	deflateEnd (out);
	free (out);

	return out_buf;
}

#if 0
/* send the a QRP table to nodes we haven't sent a real table yet */
static GtNode *update_nodes (TCPC *c, GtNode *node, void *udata)
{
	assert (node->state == GT_NODE_CONNECTED);
	assert (GT_CONN(node) == c);
	
	/* 
	 * If the counter is not 0, we sent a table to this node already.
	 * So, wait for the timer to pick that up.
	 */
	if (node->query_router_counter != 0)
		return NULL;

	/* submit the table */
	query_route_table_submit (c);

	/* reset the submit timer */
	if (GT_NODE(c)->query_route_timer != 0)
		timer_reset (GT_NODE(c)->query_route_timer);

	return NULL;
}
#endif

static void add_index (ds_data_t *key, ds_data_t *value, 
                       struct qrp_route_table *qrt)
{
	struct qrp_route_entry *entry = value->data;
	uint32_t slot;

	/* grab only the most significant bits of the entry */
	slot = entry->index >> (32 - qrt->bits);

	/* 
	 * If the entry already exists in the table, bump shared entries and
	 * forget about this entry.
	 */
	if (qrp_route_table_lookup (qrt, slot))
	{
		qrt->shared++;
		return;
	}

	qrp_route_table_insert (qrt, slot);
}

static void build_uncompressed (struct qrp_route_table *qrt)
{
	dataset_foreach (indices, DS_FOREACH(add_index), qrt);
}

static int build_qrp_table (void *udata)
{
	uint8_t   *new_table;
	size_t     new_size;
	StopWatch *sw;
	double     elapsed;
	double     fill_ratio;

	if (!route_table && !(route_table = qrp_route_table_new (MIN_TABLE_BITS)))
	{
		/* try again later */
		return TRUE;
	}

	sw = stopwatch_new (TRUE);

	/* build a table from the indices */
	build_uncompressed (route_table);

	stopwatch_stop (sw);

	elapsed = stopwatch_elapsed (sw, NULL);

	fill_ratio = qrp_route_table_fill_ratio (route_table);

	GT->DBGFN (GT, "%.4lfs elapsed building", elapsed);
	GT->DBGFN (GT, "present=%u shared=%u size=%u", route_table->present, 
	           route_table->shared, route_table->size);
	GT->DBGFN (GT, "fill ratio=%.4lf%%", fill_ratio);

	/* 
	 * If the fill ratio is greater than an acceptable threshold,
	 * and we haven't reached the maximum table size allowed,
	 * rebuild a larger routing table.
	 */
	if (fill_ratio >= INC_FILL_RATIO && route_table->bits < MAX_TABLE_BITS)
	{
		struct qrp_route_table *new_table;

		/*
		 * If we don't have enough memory to build the new table, fall
		 * through and compress the existing table. This would only happen
		 * if this node has a very small amount of memory.
		 */
		if ((new_table = qrp_route_table_new (route_table->bits + 1)))
		{
			qrp_route_table_free (route_table);
			route_table = new_table;

			/* retry the build later, it's kinda expensive */
			stopwatch_free (sw);
			return TRUE;
		}
	}

	stopwatch_start (sw);

	/* compress a new table */
	new_table = compress_table (route_table->table, 
	                            route_table->size, 
	                            &new_size);

	elapsed = stopwatch_free_elapsed (sw);

	GT->DBGFN (GT, "%.4lfs elapsed compressing", elapsed);
	GT->DBGFN (GT, "compressed size=%lu", new_size);

	if (!new_table)
		return TRUE;

	assert (new_size > 0);

	/*
	 * Replace the old compressed table
	 */
	free (compressed_table);

	compressed_table = new_table;
	compressed_size  = new_size;
	compressed_slots = route_table->slots;

	compressed_version++;

	if (!compressed_version)
		compressed_version++;

	/*
	 * An optimization to reduce memory usage: realloc the 
	 * compressed table to the smaller size.
	 */
	if ((new_table = realloc (new_table, new_size)))
		compressed_table = new_table;

#if 0
	/* update nodes with this table */
	gt_conn_foreach (GT_CONN_FOREACH(update_nodes), NULL, 
	                 GT_NODE_ULTRA, GT_NODE_CONNECTED, 0);
#endif

	/*
	 * Temporary optimization: we can free the uncompressed
	 * route table now, as it is unused. This is a dubious optimization
	 * though because the table will probably hang around in 
	 * the future when incremental updating works.
	 */
	qrp_route_table_free (route_table);
	route_table = NULL;

	/* remove the timer, as the table is now up to date */
	build_timer = 0;
	return FALSE;
}

static int start_build (void *udata)
{
	build_timer = timer_add (QRT_BUILD_INTERVAL, 
	                         (TimerCallback)build_qrp_table, NULL);
	return FALSE;
}

static void start_build_timer (void)
{
	if (build_timer)
		return;

	/*
	 * If we don't have a compressed table, we haven't built
	 * the table before, so build it soon. Otherwise,
	 * we won't submit it for a while anyway, so build it 
	 * at half the update interval.
	 */
	if (compressed_table)
	{
		build_timer = timer_add (QRT_UPDATE_INTERVAL / 2,
		                         (TimerCallback)start_build, NULL);
		return;
	}

	start_build (NULL);
}

/*****************************************************************************/

/* TODO: this should be moved to GT_SELF */
uint8_t *gt_query_router_self (size_t *size, int *version)
{
	if (!compressed_table)
		return NULL;

	assert (size != NULL && version != NULL);

	*size    = compressed_size;
	*version = compressed_version;

	return compressed_table;
}

static int free_entries (ds_data_t *key, ds_data_t *value, void *udata)
{
	struct qrp_route_entry *entry = value->data;

	free (entry);

	return DS_CONTINUE | DS_REMOVE;
}

void gt_query_router_self_destroy (void)
{
	timer_remove_zero (&build_timer);

	qrp_route_table_free (route_table);
	route_table = NULL;

	free (compressed_table);
	compressed_table   = NULL;
	compressed_slots   = 0;
	compressed_size    = 0;
	compressed_version = 0;

	dataset_foreach_ex (indices, DS_FOREACH_EX(free_entries), NULL);
	dataset_clear (indices);
	indices = NULL;
}

void gt_query_router_self_init (void)
{
	indices = dataset_new (DATASET_HASH);
}

/*****************************************************************************/

static uint32_t *append_token (uint32_t *tokens, size_t *len,
                               size_t pos, uint32_t tok)
{
	if (pos >= *len)
	{
		uint32_t *new_tokens;

		*(len) += 8;
		new_tokens = realloc (tokens, *len * sizeof (uint32_t));

		assert (new_tokens != NULL);
		tokens = new_tokens;
	}

	tokens[pos] = tok;
	return tokens;
}

static uint32_t *tokenize (char *hpath, size_t *r_len)
{
	uint32_t *tokens;
	int        count;
	size_t     len;
	char      *str, *str0;
	char      *next;

	if (!(str0 = str = STRDUP (hpath)))
		return NULL;

	tokens = NULL;
	len    = 0;
	count  = 0;

	while ((next = string_sep_set (&str, QRP_DELIMITERS)) != NULL)
	{
		uint32_t tok;

		if (string_isempty (next))
			continue;

		/* don't add keywords that are too small */
		if (strlen (next) < QRP_MIN_LENGTH)
			continue;

		tok = gt_query_router_hash_str (next, 32);
		tokens = append_token (tokens, &len, count++, tok);
	}

	*r_len = count;

	free (str0);

	return tokens;
}

void gt_query_router_self_add (FileShare *file)
{
	uint32_t               *tokens, *tokens0;
	uint32_t                tok;
	size_t                  len;
	int                     i;
	struct qrp_route_entry *entry;

	tokens0 = tokens = tokenize (share_get_hpath (file), &len);

	assert (tokens != NULL);
	assert (len > 0);

	for (i = 0; i < len; i++)
	{
		tok = tokens[i];

		if ((entry = dataset_lookup (indices, &tok, sizeof (tok))))
		{
			entry->ref++;
			continue;
		}

		/*
		 * Create a new index and add it to the table.
		 */
		if (!(entry = malloc (sizeof (struct qrp_route_entry))))
			continue;

		entry->ref   = 1;
		entry->index = tok;

		dataset_insert (&indices, &tok, sizeof (tok), entry, 0);

		table_changed = TRUE;
	}

	free (tokens0);
}

void gt_query_router_self_remove (FileShare *file)
{
	uint32_t               *tokens, *tokens0;
	uint32_t                tok;
	size_t                  len;
	int                     i;
	struct qrp_route_entry *entry;

	tokens0 = tokens = tokenize (share_get_hpath (file), &len);

	assert (tokens != NULL);
	assert (len > 0);

	for (i = 0; i < len; i++)
	{
		tok = tokens[i];

		entry = dataset_lookup (indices, &tok, sizeof (tok));
		assert (entry != NULL);

		if (--entry->ref > 0)
			continue;

		dataset_remove (indices, &tok, sizeof (tok));

		table_changed = TRUE;
	}

	free (tokens0);
}

void gt_query_router_self_sync (BOOL begin)
{
	if (!begin && table_changed)
	{
		start_build_timer();
		table_changed = FALSE;
	}
}

/*****************************************************************************/

int query_patch_open (GtQueryRouter *router, int seq_size, int compressed,
                      size_t max_size)
{
	GtQueryPatch *new_patch;

	if (!(new_patch = malloc (sizeof (GtQueryPatch))))
		return FALSE;

	memset (new_patch, 0, sizeof (GtQueryPatch));

	if (!(new_patch->stream = zlib_stream_open (max_size)))
	{
		free (new_patch);
		return FALSE;
	}

	new_patch->seq_size   = seq_size;
	new_patch->compressed = compressed;

	/* NOTE: sequence is 1-based, not 0-based */
	new_patch->seq_num = 1;

	router->patch = new_patch;

	return TRUE;
}

void query_patch_close (GtQueryRouter *router)
{
	GtQueryPatch *patch;

	GT->DBGFN (GT, "entered");

	if (!router)
		return;

	patch = router->patch;

	if (!patch)
		return;

	zlib_stream_close (patch->stream);

	router->patch = NULL;
	free (patch);
}

/* TODO: compact router tables to bit-level */
static void query_patch_apply (GtQueryRouter *router, int bits, char *data,
                               size_t data_size)
{
	GtQueryPatch   *patch;
	char           *table;          /* NOTE: this must be signed */
	int             i;

	patch = router->patch;
	assert (patch != NULL);

	/* check for overflow: this may look wrong but its not */
	if (patch->table_pos + (data_size - 1) >= router->size)
	{
		GT->DBGFN (GT, "patch overflow: %u (max of %u)",
		           patch->table_pos+data_size, router->size);
		query_patch_close (router);
		return;
	}

	table = router->table;

	/* hrm */
	if (bits == 4)
	{
		int j;

		for (i = 0; i < data_size; i++)
		{
			int pos;
			char change;

			pos = i + patch->table_pos;

			/* avoid % */
			j = (i+1) & 0x1;

			/* grab the correct half of the byte and sign-extend it
			 * NOTE: this starts off with the most significant bits! */
			change = data[i] & (0x0f << (4 * j));

			/* move to least significant bits
			 * TODO: does this do sign-extension correctly? */
			change >>= 4;

			table[pos] += change;
		}
	}
	else if (bits == 8)
	{
		/* untested */
		for (i = 0; i < data_size; i++)
		{
			table[i + patch->table_pos] += data[i];
		}
	}
	else
	{
		GT->DBGFN (GT, "undefined bits value in query patch: %u", bits);
		query_patch_close (router);
		return;
	}

	/* store the table position for the next patch */
	patch->table_pos += i;

	/* cleanup the data if the patch is done */
	patch->seq_num++;

	if (patch->seq_num > patch->seq_size)
		query_patch_close (router);
}

/*****************************************************************************/

/* TODO: compact router tables to bit-level */
GtQueryRouter *gt_query_router_new (size_t size, int infinity)
{
	GtQueryRouter *router;

	if (size > MAX_TABLE_SIZE)
		return NULL;

	if (!(router = malloc (sizeof (GtQueryRouter))))
		return NULL;

	memset (router, 0, sizeof (GtQueryRouter));

	if (!(router->table = malloc (size)))
	{
		free (router->table);
		return NULL;
	}

	memset (router->table, infinity, size);

	router->size = size;

	return router;
}

void gt_query_router_free (GtQueryRouter *router)
{
	if (!router)
		return;

	query_patch_close (router);

	free (router->table);
	free (router);
}

/*****************************************************************************/

static void print_hex (unsigned char *data, size_t size)
{
	fprint_hex (stdout, data, size);
}

void gt_query_router_update (GtQueryRouter *router, size_t seq_num,
                             size_t seq_size, int compressed, int bits,
                             unsigned char *zdata, size_t size)
{
	GtQueryPatch *patch;
	char         *data;

	if (!router)
	{
		GT->DBGFN (GT, "null router");
		return;
	}

	if (!router->patch)
	{
		if (!query_patch_open (router, seq_size, compressed, router->size))
			return;
	}

	patch = router->patch;

	/* check for an invalid sequence number or size */
	if (patch->seq_size != seq_size || patch->seq_num != seq_num)
	{
		GT->DBGFN (GT, "bad patch: seq_size %u vs %u, seq_num %u vs %u",
		           patch->seq_size, seq_size, patch->seq_num, seq_num);
		query_patch_close (router);
		return;
	}

	if (compressed != patch->compressed)
	{
		GT->DBGFN (GT, "tried to change encodings in patch");
		query_patch_close (router);
		return;
	}

	switch (compressed)
	{
	 case 0x00: /* no compression */
		if (!zlib_stream_write (patch->stream, zdata, size))
		{
			GT->DBGFN (GT, "error copying data");
			query_patch_close (router);
			return;
		}

		break;

	 case 0x01: /* deflate */
		printf ("zlib compressed data:\n");
		print_hex (zdata, size);

		if (!zlib_stream_inflate (patch->stream, zdata, size))
		{
			GT->DBGFN (GT, "error inflating data");
			query_patch_close (router);
			return;
		}

		break;

	 default:
		GT->DBGFN (GT, "unknown compression algorithm in query route patch");
		return;
	}

	/* read the data in the stream */
	if (!(size = zlib_stream_read (patch->stream, &data)))
	{
		GT->DBGFN (GT, "error calling zlib_stream_read");
		query_patch_close (router);
		return;
	}

	printf ("after uncompressing:\n");
	print_hex (data, size);

	/* apply the patch -- this will cleanup any data if necessary */
	query_patch_apply (router, bits, data, size);

	print_hex (router->table, router->size);
}

/*****************************************************************************/

static void submit_empty_table (TCPC *c)
{
	static char table[8] = { 0 };
	int         len;

#if 0
	size_t      size;
#endif

	GT->DBGFN (GT, "reseting route table for %s", net_ip_str (GT_NODE(c)->ip));

	/* all slots in the table should be initialized to infinity, so it
	 * should be "empty" on the remote node */
	memset (table, 0, sizeof (table));


#if 0
	/* TEST: set part of the table to -infinity to get queries */
	size = sizeof (table);
	memset (table + (size + 1) / 2 - 1, -infinity, (size + 1) / 4);
#endif

	/* format: <query-route-msg-type> <length of table> <infinity> */
	if (gt_packet_send_fmt (c, GT_MSG_QUERY_ROUTE, NULL, 1, 0,
	                        "%c%lu%c", 0, (unsigned long) sizeof (table),
	                        INFINITY) < 0)
	{
		GT->DBGFN (GT, "error reseting table");
		return;
	}

	len = sizeof (table);

	if (gt_packet_send_fmt (c, GT_MSG_QUERY_ROUTE, NULL, 1, 0,
	                        "%c%c%c%c%c%*p",
	                        1, 1, 1, 0, 8, len, table) < 0)
	{
		GT->DBGFN (GT, "error sending empty patch");
		return;
	}
}

static void submit_table (TCPC *c, uint8_t *table, size_t size, size_t slots)
{
	int       infinity   = INFINITY;
	int       seq_size;
	int       compressed;
	int       seq_num;
	uint8_t  *p;
	size_t    send_size;

	/* XXX make table size settable at runtime */

	/* send a reset table first */
	if (gt_packet_send_fmt (c, GT_MSG_QUERY_ROUTE, NULL, 1, 0,
	                        "%c%lu%c", 0, (long)slots, infinity) < 0)
	{
		GT->DBGFN (GT, "error reseting table");
		return;
	}

	/* Break the table into PATCH_FRAGSIZE-sized chunks,
	 * and include any leftover portions. */
	seq_size = size / PATCH_FRAGSIZE +
	           (size % PATCH_FRAGSIZE == 0 ? 0 : 1);

	assert (seq_size < 256);
#if 0
	GT->dbg (GT, "sequence size: %u", seq_size);
#endif

	p = table;
	compressed = TRUE;

	/* NOTE: patch sequence numbers start at 1 */
	for (seq_num = 1; seq_num <= seq_size; seq_num++)
	{
		send_size = MIN (PATCH_FRAGSIZE, size);

		if (gt_packet_send_fmt (c, GT_MSG_QUERY_ROUTE, NULL, 1, 0,
		                        "%c%c%c%c%c%*p",
		                        /* QRP PATCH */ 1,
		                        seq_num, seq_size, compressed,
		                        /* bits */ PATCH_BITS,
		                        send_size, p) < 0)
		{
			GT->DBGFN (GT, "error sending QRT patch");
			return;
		}

		size -= send_size;
		p += send_size;
	}
}

static BOOL update_qr_table (TCPC *c)
{
	size_t     size;
	int        version;
	uint8_t   *table;
	GtNode    *node       = GT_NODE(c);

	assert (node->state & GT_NODE_CONNECTED);

	table = gt_query_router_self (&size, &version);

	/* we may not have finished building a table yet */
	if (!table)
		return TRUE;

	/* dont submit a table if this node is already up to date */
	if (node->query_router_counter == version)
		return TRUE;

	/* HACK: this shouldn't be using compressed_slots */
	submit_table (c, table, size, compressed_slots);

	/* store the version number of this table so
	 * we dont resubmit unecessarily */
	node->query_router_counter = version;

	return TRUE;
}

static BOOL submit_first_table (TCPC *c)
{
	GtNode *node = GT_NODE(c);

	assert (node->state & GT_NODE_CONNECTED);

	update_qr_table (c);

	/* remove the first timer */
	timer_remove (node->query_route_timer);

	/* set the timer for updating the table repeatedly */
	node->query_route_timer = timer_add (QRT_UPDATE_INTERVAL, 
	                                     (TimerCallback)update_qr_table, c);

	/* we removed the timer, and must return FALSE */
	return FALSE;
}

/*
 * Submit the query routing table for this node to another.
 * 
 * This delays sending the table for while. This helps preserve our precious
 * upstream when we're looking for nodes to connect to, as this often
 * happens when we're in the middle of looking for more nodes.
 */
void query_route_table_submit (TCPC *c)
{
	GtNode *node = GT_NODE(c);

	assert (node->query_route_timer == 0);

	/* save bandwidth with an empty table */
	submit_empty_table (c);

	/* submit a real table later */
	node->query_route_timer = timer_add (QRT_SUBMIT_DELAY,
	                                     (TimerCallback)submit_first_table, c);
}

/*****************************************************************************/
/* TESTING */

#if 0
#define CHECK(x) do { \
	if (!(x)) printf("FAILED: %s\n", #x); \
	else printf("OK: %s\n", #x); \
} while (0)

#define HASH(str, bits) \
	printf ("hash " str ": %u\n", gt_query_router_hash_str (str, bits))

int main (int argc, char **argv)
{
#define hash gt_query_router_hash_str

	CHECK(hash("", 13)==0);
	CHECK(hash("eb", 13)==6791);
	CHECK(hash("ebc", 13)==7082);
	CHECK(hash("ebck", 13)==6698);
	CHECK(hash("ebckl", 13)==3179);
	CHECK(hash("ebcklm", 13)==3235);
	CHECK(hash("ebcklme", 13)==6438);
	CHECK(hash("ebcklmen", 13)==1062);
	CHECK(hash("ebcklmenq", 13)==3527);
	CHECK(hash("", 16)==0);
	CHECK(hash("n", 16)==65003);
	CHECK(hash("nd", 16)==54193);
	CHECK(hash("ndf", 16)==4953);
	CHECK(hash("ndfl", 16)==58201);
	CHECK(hash("ndfla", 16)==34830);
	CHECK(hash("ndflal", 16)==36910);
	CHECK(hash("ndflale", 16)==34586);
	CHECK(hash("ndflalem", 16)==37658);
	CHECK(hash("FAIL", 16)==37458);	// WILL FAIL
	CHECK(hash("ndflaleme", 16)==45559);
	CHECK(hash("ol2j34lj", 10)==318);
	CHECK(hash("asdfas23", 10)==503);
	CHECK(hash("9um3o34fd", 10)==758);
	CHECK(hash("a234d", 10)==281);
	CHECK(hash("a3f", 10)==767);
	CHECK(hash("3nja9", 10)==581);
	CHECK(hash("2459345938032343", 10)==146);
	CHECK(hash("7777a88a8a8a8", 10)==342);
	CHECK(hash("asdfjklkj3k", 10)==861);
	CHECK(hash("adfk32l", 10)==1011);
	CHECK(hash("zzzzzzzzzzz", 10)==944);

	CHECK(hash("3nja9", 10)==581);
	CHECK(hash("3NJA9", 10)==581);
	CHECK(hash("3nJa9", 10)==581);

	printf ("hash(FAIL, 16) = %u\n", hash("FAIL", 16));
	return 0;
}
#endif
