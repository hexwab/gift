/*
 * $Id: gt_stats.c,v 1.20 2004/01/29 07:52:19 hipnod Exp $
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

#include "gt_node.h"
#include "gt_node_list.h"

#include "gt_search.h"

/*****************************************************************************/

struct gt_stats
{
	double          size_kb;
	unsigned long   files;
	size_t          users;
};

/* number of stats samples to accumulate */
#define NR_SAMPLES         64

/* number of samples to take surrounding the median */
#define NR_MEDIAN_SAMPLES  2

/*****************************************************************************/

/* 
 * This should be computed from our neighboring peers, but for now it
 * hardcodes the max leaves for the most numerous nodes on the network,
 * Limewire (32) and BearShare (10) 
 */
static int              avg_leaves     = (30 + 32) / 2;

/* default ttl we use in searches */
static int              default_ttl    = GT_SEARCH_TTL;

/* default outdegree for most nodes */
static int              default_degree = 6;

/* keep track of the stats for the last NR_SAMPLES pongs */
static struct gt_stats  samples[NR_SAMPLES];
static size_t           samples_index;
static size_t           samples_count;

/*****************************************************************************/

void gt_stats_accumulate (in_addr_t ipv4, in_port_t port,
                          in_addr_t src_ip, uint32_t files,
                          uint32_t size_kb)
{
	samples[samples_index].files   = files;
	samples[samples_index].size_kb = size_kb;

	samples_index = (samples_index + 1) % NR_SAMPLES;
	samples_count++;

	if (samples_count > NR_SAMPLES)
		samples_count = NR_SAMPLES;
}

static int stats_cmp (const void *a, const void *b)
{
	const struct gt_stats *a_s = a;
	const struct gt_stats *b_s = b;

	return a_s->size_kb - b_s->size_kb;
}

static void clear_stats (struct gt_stats *stats)
{
	stats->size_kb = 0.0;
	stats->files   = 0;
	stats->users   = 0;
}

static void get_median_stats (struct gt_stats *pong_stats, size_t nr)
{
	int low, high;
	int mid;
	int i;

	if (nr == 0)
		return;

	mid  = nr / 2;

	low  = mid - NR_MEDIAN_SAMPLES;
	high = mid + NR_MEDIAN_SAMPLES;

	if (low < 0)
		low = 0;
	
	if (high > nr - 1)
		high = nr - 1;

	for (i = low; i <= high; i++)
	{
		pong_stats->size_kb += samples[i].size_kb;
		pong_stats->files   += samples[i].files;
		pong_stats->users++;
	}
}

static void get_pong_stats (struct gt_stats *pong_stats)
{
	qsort (samples, samples_count, sizeof (struct gt_stats), stats_cmp);

	clear_stats (pong_stats);
	get_median_stats (pong_stats, samples_count);
}

/*****************************************************************************/

static TCPC *count_stats (TCPC *c, GtNode *node, struct gt_stats *st)
{
	/* sanity check */
	if (node->size_kb == (uint32_t)-1 || node->files == (uint32_t)-1)
		return NULL;

	st->size_kb += node->size_kb;
	st->files   += node->files;

	if (node->vitality > 0)
		st->users++;

	return NULL;
}

static void get_conn_stats (struct gt_stats *conn_stats)
{
	clear_stats (conn_stats);

	/* grab statistics from the nodes structures */
	gt_conn_foreach (GT_CONN_FOREACH(count_stats), conn_stats,
	                 GT_NODE_NONE, GT_NODE_ANY, 0);
}

/*****************************************************************************/

/* no need to pull in math.h for this simple thing: exponent is small */
static unsigned long int_pow (int base, int exponent)
{
	unsigned long total = 1;

	while (exponent-- > 0)
		total *= base;

	return total;
}

/* TODO: do this on a per-node basis, and use X-MaxTTL and X-Degree */
static unsigned long sum_network (int degree, int ttl)
{
	int            i;
	unsigned long  sum;

	if (ttl <= 0)
		return 0;

	sum = degree;

	for (i = 2; i <= ttl; i++)
		sum += int_pow (degree-1, i-1) * degree;

	return sum;
}

/* 
 * Of course this is totally inaccurate, and not even dynamic. It
 * approximates the maximum number of users in a network of
 * Gnutella's structure in a given outdegree and TTL range
 * (default_degree, default_ttl). This doesn't represent the whole
 * network, and since the network has a significant amount of
 * cycles this calculation is likely way too much.
 *
 * To compensate, we divide by 3. We could make a better guess
 * about the number of nodes if we could find the number of cycles
 * some way, but this information is not readily available.
 */
static unsigned long guess_users (size_t connected)
{
	unsigned long users;

	users = sum_network (default_degree, default_ttl) * connected;
	
	/* multiply by the number of leaves */
	users *= avg_leaves;

	/* divide by redundancy level */
	users /= 3;

	/* divide by 3 to account for cycles */
	users /= 3;

	/* multiply by 2 because, well...this whole thing is misleading anyway,
	 * and the total number of users is greater than the horizon */
	users *= 2;

	return users;
}

/*****************************************************************************/

/*
 * TODO: accumulate statistics on average leaves/cycles on our immediate
 *       peers.
 */
int gnutella_stats (Protocol *p, unsigned long *users, unsigned long *files,
                    double *size, Dataset **extra)
{
	struct gt_stats pong_stats;
	struct gt_stats conn_stats;
	struct gt_stats avg_stats;
	size_t          connected;

	*files = *users = *size = 0;

	connected = gt_conn_length (GT_NODE_ULTRA, GT_NODE_CONNECTED);

	if (connected == 0)
		return 0;

	get_pong_stats (&pong_stats);
	get_conn_stats (&conn_stats);

	if (conn_stats.users == 0)
		conn_stats.users = 1;

	if (pong_stats.users == 0)
		pong_stats.users = 1;

	/* divide the total files size by two since it is inflated by ultrapeers
	 * abusing it to communicate their ultrapeer-ness to other nodes */
	pong_stats.size_kb /= 2;
	conn_stats.size_kb /= 2;

	/* find the average of the data for our two sources */
	pong_stats.size_kb /= pong_stats.users;
	pong_stats.files   /= pong_stats.users;
	conn_stats.size_kb /= conn_stats.users;
	conn_stats.files   /= conn_stats.users;

	/* put the stats of previously connected nodes on equal footing with the
	 * stats collected from pongs -- they should be more reliable */
	avg_stats.files   = (pong_stats.files + conn_stats.files) / 2;
	avg_stats.size_kb = (pong_stats.size_kb + conn_stats.size_kb) / 2;

	/* add conn_stats.users for a little variation (not much) */
	avg_stats.users = guess_users (connected) + conn_stats.users;

	*users = avg_stats.users;
	*files = avg_stats.files * avg_stats.users;
	*size  = avg_stats.size_kb * avg_stats.users / 1024 / 1024;

	return connected;
}
