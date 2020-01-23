/*
 * $Id: tx_packet.c,v 1.9 2004/05/02 08:55:00 hipnod Exp $
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
#include "gt_packet.h"       /* packet manipulation macros */

#include "io/tx_stack.h"
#include "io/tx_layer.h"
#include "io/io_buf.h"

/*****************************************************************************/

#define TX_PACKET_DEBUG    0

/*****************************************************************************/

/*
 * Relative packet ratios.  These control how many packets of a certain type
 * are sent prior to looking for other types.  For each type we maintain an
 * independent FIFO queue.  Each time a packet can be sent, each queue is
 * checked.  If there is no packet present in a queue, the ratio for that
 * particular queue is set to zero.
 *
 * Otherwise, we try to send packets while the ratio is greater than zero and
 * there are packets on the queue.  Once the ratio hits zero, that queue is
 * done and the next queue will be checked.  When all packet ratios are zero,
 * the ratios are reset.  This process continues until the lower layer becomes
 * saturated (returns short or EAGAIN/EWOULDBLOCK).
 *
 * Note that it is very important to decrement the packet ratios even if there
 * are no packets of a particular type present, because otherwise never
 * sending one particular packet type could lead to us never reseting ratios,
 * and thus starving all other packet types -- since reseting the ratios
 * depends on all the ratios going to zero.
 *
 * Since the queues are checked in this order, this gives each packet type a
 * priority.  Pushes are the most important type, because they need help.
 */
#define PUSH_RATIO    5
#define QHIT_RATIO    4
#define QUERY_RATIO   3
#define PONG_RATIO    2
#define PING_RATIO    1
#define MISC_RATIO    1

#define NR_QUEUES    (6)

/*****************************************************************************/

struct packet_queue
{
	gt_packet_type_t msg_type;
	size_t           ratio;        /* how many packets left on this turn? */
	size_t           bytes_queued; /* total bytes queued */
	List            *queue;
};

struct tx_packet
{
	struct packet_queue urgent;             /* keep-alive PINGs, BYE */
	struct packet_queue queues[NR_QUEUES];
	int                 total_pkts;         /* used to quickly test if empty */
};

/*****************************************************************************/
/* DEBUGGING/TRACING */

#if TX_PACKET_DEBUG
/* ripped from gt_packet.c */
static const char *packet_command_str (uint8_t cmd)
{
	static char buf[16];

	switch (cmd)
	{
	 case GT_MSG_PING:        return "PING";
	 case GT_MSG_PING_REPLY:  return "PONG";
	 case GT_MSG_BYE:         return "BYE";
	 case GT_MSG_QUERY_ROUTE: return "QROUTE";
	 case GT_MSG_VENDOR:      return "VMSG";
	 case GT_MSG_VENDOR_STD:  return "VMSG-S";
	 case GT_MSG_PUSH:        return "PUSH";
	 case GT_MSG_QUERY:       return "QUERY";
	 case GT_MSG_QUERY_REPLY: return "HITS";

	 default:
		snprintf (buf, sizeof (buf), "[<%02hx>]", cmd);
		return buf;
	}
}

static void dump_packet (struct io_buf *buf, String *str)
{
	uint8_t cmd = get_command (buf->data);
	string_appendf (str, "%s,", packet_command_str (cmd));
}

static void trace_queue_list (List *queue, String *str)
{
	list_foreach (queue, (ListForeachFunc)dump_packet, str);
}
#endif /* TX_PACKET_DEBUG */

static void trace_queue (struct tx_layer *tx, const char *id)
{
#if TX_PACKET_DEBUG
	struct tx_packet *tx_packet = tx->udata;
	int i;
	String *s;
	TCPC *c;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return;

	c = tx->stack->c;
	string_appendf (s, "{%s totalpkts=%d ", id, tx_packet->total_pkts);

	/* trace urgent */
	trace_queue_list (tx_packet->urgent.queue, s);

	for (i = 0; i < NR_QUEUES; i++)
		trace_queue_list (tx_packet->queues[i].queue, s);

	string_append (s, "}");

	GT->DBGSOCK (GT, c, "%s", s->str);
	string_free (s);
#endif
}

/*****************************************************************************/

/* return the queue on which this message should go */
static size_t get_queue (struct io_buf *msg)
{
	uint8_t cmd;

	cmd = get_command (msg->data);

	switch (cmd)
	{
	 default:
		abort ();

	 case GT_MSG_VENDOR:
	 case GT_MSG_VENDOR_STD:
	 case GT_MSG_QUERY_ROUTE:
		return 6;

	 case GT_MSG_PING:
		{
			/* queue keep-alive pings in the urgent queue */
			if (get_ttl (msg->data) == 1 && get_hops (msg->data) == 0)
				return 0;
		}
		return 5;

	 case GT_MSG_PING_REPLY:
		{
			/*
			 * Queue replies to keep-alive ping in the urgent queue.
			 *
			 * This allows the remote end to starve it's own connection
			 * with a series of keep-alive pings.  Only flow-control
			 * can handle this.
			 */
			if (get_ttl (msg->data) == 1 && get_hops (msg->data) == 0)
				return 0;
		}
		return 4;

	 case GT_MSG_QUERY:
		{
			/* make queries from this node more important */
			if (get_ttl (msg->data) == 1 && get_hops (msg->data) == 0)
				return 1;
		}
		return 3;

	 case GT_MSG_QUERY_REPLY:
		return 2;

	 case GT_MSG_PUSH:
		return 1;

	 case GT_MSG_BYE:
		return 0;
	}

	abort ();
}

static void enqueue_packet (struct packet_queue *pkt_queue, struct io_buf *msg)
{
	pkt_queue->queue = list_append (pkt_queue->queue, msg);
}

/*
 * Called from upper layer when it wants to send us a message buffer.
 */
static tx_status_t tx_packet_queue (struct tx_layer *tx, struct io_buf *io_buf)
{
	struct tx_packet    *tx_packet = tx->udata;
	struct packet_queue *queue;
	size_t queue_nr;

	queue_nr = get_queue (io_buf);

	if (queue_nr == 0)
	{
		queue = &tx_packet->urgent;
	}
	else
	{
		assert (queue_nr <= 6);
		queue = &tx_packet->queues[queue_nr - 1];
	}

	enqueue_packet (queue, io_buf);

	tx_packet->total_pkts++;
	assert (tx_packet->total_pkts > 0);

	trace_queue (tx, "*0*");

	return TX_OK;
}

/*****************************************************************************/

/* helper func */
static void set_queue (struct packet_queue *queue, gt_packet_type_t msg_type,
                       size_t prio)
{
	queue->msg_type = msg_type;
	queue->ratio    = prio;
}

/* reset the ratios for the packet queue */
static void reset_ratios (struct packet_queue *queue, size_t len)
{
	/*
	 * These are arranged in order of relative priority.
	 */
	set_queue (&queue[0], GT_MSG_PUSH,        PUSH_RATIO);
	set_queue (&queue[1], GT_MSG_QUERY_REPLY, QHIT_RATIO);
	set_queue (&queue[2], GT_MSG_QUERY,       QUERY_RATIO);
	set_queue (&queue[3], GT_MSG_PING_REPLY,  PONG_RATIO);
	set_queue (&queue[4], GT_MSG_PING,        PING_RATIO);
	set_queue (&queue[5], 0xff,               MISC_RATIO);
}

/*****************************************************************************/

/*
 * Try to send a single message buffer from the packet queue to the lower
 * layer.  If the lower layer has become saturated, return FALSE.
 *
 * The lower layer takes responsibility for the messages sent to it in
 * entirety, but will return less than the total message length if it has
 * become saturated.
 */
static tx_status_t shift_queue (struct tx_layer *tx,
                                struct tx_packet *tx_packet,
                                struct packet_queue *pkt_queue)
{
	List          *msg_l;
	struct io_buf *msg;
	tx_status_t    ret;

	msg_l = list_nth (pkt_queue->queue, 0);
	msg   = msg_l->data;

	ret = gt_tx_layer_queue (tx, msg);

	if (ret != TX_OK)
	{
		assert (ret != TX_EMPTY); /* impossible to be empty */
		return ret;
	}

	/* shift this packet off the queue */
	pkt_queue->queue = list_remove_link (pkt_queue->queue, msg_l);

	tx_packet->total_pkts--;
	assert (tx_packet->total_pkts >= 0);

	if (TX_PACKET_DEBUG)
		trace_queue (tx, "*2*");

	return ret;
}

static tx_status_t service_queues (struct tx_layer *layer,
                                   struct tx_packet *tx_packet)
{
	int         i;
	tx_status_t ret;

	for (i = 0; i < NR_QUEUES; i++)
	{
		struct packet_queue *pkt_queue  = &tx_packet->queues[i];

		/* skip if ratio is small */
		while (pkt_queue->ratio > 0 && pkt_queue->queue != NULL)
		{
			ret = shift_queue (layer, tx_packet, pkt_queue);
			pkt_queue->ratio--;

			if (ret == TX_FULL)
				return TX_OK;

			if (ret != TX_OK)
			    return ret;
		}
	}

	/* reset the ratios to write more data */
	reset_ratios (tx_packet->queues, NR_QUEUES);

	/* we wrote something, so return ok */
	if (tx_packet->total_pkts == 0)
		return TX_OK;

	/* tail recurse until lower layer is saturated */
	return service_queues (layer, tx_packet);
}

/*
 * Gets called when the lower layer is writable.
 */
static tx_status_t tx_packet_ready (struct tx_layer *tx)
{
	struct tx_packet *tx_packet = tx->udata;
	tx_status_t       ret;

	if (tx_packet->total_pkts == 0)
		return TX_EMPTY;

	if (TX_PACKET_DEBUG)
		trace_queue (tx, "*1*");

	/* check the urgent queue first */
	while (tx_packet->urgent.queue != NULL)
	{
		ret = shift_queue (tx, tx_packet, &tx_packet->urgent);

		if (ret == TX_FULL)
			return TX_OK;

		if (ret != TX_OK)
			return ret;

		if (TX_PACKET_DEBUG)
			trace_queue (tx, "*3*");
	}

	return service_queues (tx, tx_packet);
}

/*****************************************************************************/

static BOOL tx_packet_init (struct tx_layer *tx)
{
	struct tx_packet *tx_packet;

	if (!(tx_packet = NEW (struct tx_packet)))
		return FALSE;

	reset_ratios (tx_packet->queues, NR_QUEUES);

	tx->udata = tx_packet;

	return TRUE;
}

static BOOL free_io_buf (struct io_buf *io_buf, void *udata)
{
	io_buf_free (io_buf);
	return TRUE;
}

static void flush_packets (struct packet_queue *pkt_queue)
{
	list_foreach_remove (pkt_queue->queue, (ListForeachFunc)free_io_buf, NULL);
	pkt_queue = NULL;
}

static void tx_packet_destroy (struct tx_layer *tx)
{
	struct tx_packet *tx_packet = tx->udata;
	int i;

	flush_packets (&tx_packet->urgent);

	for (i = 0; i < NR_QUEUES; i++)
		flush_packets (&tx_packet->queues[i]);

	FREE (tx_packet);
}

static void tx_packet_consume (struct tx_layer *tx, BOOL stop)
{
	/* nothing */
}

/*****************************************************************************/

static void tx_packet_enable (struct tx_layer *tx)
{
	/* TODO */
}

static void tx_packet_disable (struct tx_layer *tx)
{
	/* TODO */
}

/*****************************************************************************/

struct tx_layer_ops gt_tx_packet_ops =
{
	tx_packet_init,
	tx_packet_destroy,
	tx_packet_consume,
	tx_packet_queue,
	tx_packet_ready,
	tx_packet_enable,
	tx_packet_disable,
};
