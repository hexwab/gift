/*
 * $Id: as_push_reply.c,v 1.4 2004/12/19 19:19:01 mkern Exp $
 *
 * Copyright (C) 2004 giFT-Ares project
 * http://developer.berlios.de/projects/gift-ares
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

#include "as_ares.h"

/*****************************************************************************/

typedef struct
{
	/* TCP connection and for this reply. */
	TCPC *c;

	/* Copy of share we want to push. */
	ASShare *share;

	/* Id we forward to pushee so it can match the reply to its request. */
	char *id_str;

	/* Pointer to manager for this push. */
	ASPushReplyMan *man;

} ASPushReply;

/*****************************************************************************/

static ASPushReply *as_pushreply_create ()
{
	ASPushReply *reply;

	if (!(reply = malloc (sizeof (ASPushReply))))
		return NULL;
	
	reply->c = NULL;

	reply->share = NULL;
	reply->id_str = NULL;
	reply->man = NULL;

	return reply;
}

static void as_pushreply_free (ASPushReply *reply, as_bool close_conn)
{
	if (!reply)
		return;

	/* Removes all inputs as well. */
	if (close_conn && reply->c)
		tcp_close (reply->c);

	as_share_free (reply->share);
	free (reply->id_str);
	free (reply);
}

/*****************************************************************************/

/* Allocate and init push reply manager. */
ASPushReplyMan *as_pushreplyman_create ()
{
	ASPushReplyMan *man;

	if (!(man = malloc (sizeof (ASPushReplyMan))))
		return NULL;

	man->replies = NULL;

	return man;
}

static int free_reply_itr (ASPushReply *reply, void *udata)
{
	as_pushreply_free (reply, TRUE);

	return TRUE; /* remove */
}

/* Free manager. */
void as_pushreplyman_free (ASPushReplyMan *man)
{
	if (!man)
		return;

	man->replies = list_foreach_remove (man->replies,
	                                    (ListForeachFunc)free_reply_itr,
	                                    NULL);
	free (man);
}

/*****************************************************************************/

static void pushreply_connected (int fd, input_id input, ASPushReply *reply)
{
	ASPushReplyMan *man = reply->man;
	TCPC *c = reply->c;
	char *req, *hexhash;

	input_remove (input);

	/* Remove reply from manager's list. */
	man->replies = list_remove (man->replies, reply);
	reply->man = NULL;

	if (net_sock_error (fd))
	{
		AS_DBG_2 ("Push reply connect to %s:%d failed",
		          net_ip_str (c->host), c->port);
		as_pushreply_free (reply, TRUE);
		return;
	}

	/* Assemble push message. */
	hexhash = as_hex_encode (reply->share->hash->data, AS_HASH_SIZE);
	req = stringf ("PUSH SHA1:%s%s\n\n", hexhash, reply->id_str);
	free (hexhash);

	/* Send message. */
	if (tcp_send (c, req, strlen (req)) != strlen (req))
	{
		AS_DBG_2 ("Push reply send to %s:%d failed", net_ip_str (c->host),
		          c->port);
		as_pushreply_free (reply, TRUE);
		return;
	}

	/* Free reply but keep connection alive. */
	as_pushreply_free (reply, FALSE);

	AS_DBG_1 ("Push to %s succeeded.", net_peer_ip (c->fd));

	/* Hand off connection to http server if it still exists. */
	if (AS->server)
		as_http_server_pushed (AS->server, c);
	else
		tcp_close (c);
}

/*****************************************************************************/

/* Handle push reply request by creating internal reply object and starting
 * connection to host. If the connections succeeds it will be handed over to
 * the http server.
 */
void as_pushreplyman_handle (ASPushReplyMan *man, ASPacket *packet)
{
	in_addr_t ip;
	in_port_t port;
	ASHash *hash;
	ASPushReply *reply;
	int c;

#ifdef HEAVY_DEBUG
	as_packet_dump (packet);
#endif

	if (!AS->server)
	{
		AS_DBG ("Ignoring push request because http server is down.");
		return;
	}

	if (as_packet_remaining (packet) < 35)
	{
		AS_DBG_1 ("Truncated push request (%d bytes)",
		          as_packet_remaining (packet));
		return;
	}

	/* Get ip, port and hash from request. */
	ip = as_packet_get_ip (packet);
	port = as_packet_get_le16 (packet);

	/* Play it safe. */
	if (port < 1024)
	{
		AS_DBG_1 ("Denying push reuquest to port %d < 1024.", port);
		return;
	}
	
	if (!(hash = as_packet_get_hash (packet)))
	{
		AS_DBG ("Couldn't create hash from push request.");
		return;
	}

	/* Create reply object. */
	if (!(reply = as_pushreply_create ()))
	{
		as_hash_free (hash);
		return;
	}

	/* Check that we are actually sharing the requested file. */
	if (!(reply->share = as_shareman_lookup (AS->shareman, hash)))
	{
		AS_DBG_3 ("Unknown share '%s' for push request to %s:%d.",
		          as_hash_str (hash), net_ip_str (ip), port);
		as_pushreply_free (reply, TRUE);
		as_hash_free (hash);
		return;
	}

	as_hash_free (hash);

	/* We don't know what this is but don't process rest if it doesn't
	 * match.
	 */
	if ((c = as_packet_get_8 (packet)) != 0x00)
	{
		AS_WARN_3 ("Unexpected value (0x%02X) for unknown byte in push request from %s:%d.",
		           c, net_ip_str (ip), port);
		as_pushreply_free (reply, TRUE);
		return;
	}

	/* Get push id string. */
	reply->id_str = as_packet_get_str (packet, 8);

	if (!reply->id_str || strlen (reply->id_str) != 8)
	{
		AS_DBG_2 ("Invalid id string ('%s', len %d) in push request.",
		          reply->id_str, strlen (reply->id_str));
		as_pushreply_free (reply, TRUE);
		return;
	}

	/* Create TCP connection. */
	if (!(reply->c = tcp_open (ip, port, FALSE)))
	{
		AS_DBG_2 ("Couldn't open connection to %s:%d for sending push reply.",
		          net_ip_str (ip), port);
		as_pushreply_free (reply, TRUE);
		return;
	}

	/* Reference the reply to manager and add it to list. */
	reply->man = man;
	man->replies = list_prepend (man->replies, reply);

	AS_DBG_3 ("Pushing '%s' to %s:%d.", as_hash_str (reply->share->hash),
	          net_ip_str (ip), port);

	/* Wait for connection. */
	input_add (reply->c->fd, reply, INPUT_WRITE,
	           (InputCallback)pushreply_connected, AS_PUSH_CONNECT_TIMEOUT);
}

/*****************************************************************************/
