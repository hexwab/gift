/*
 * $Id: as_incoming.c,v 1.10 2005/11/08 20:17:32 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

int as_incoming_http (ASHttpServer *server, TCPC *tcpcon,
                      ASHttpHeader *request)
{
	assert (AS->upman);
	as_upman_start (AS->upman, tcpcon, request);

	return TRUE; /* as_upman_start always handles connection and request */
}

/*****************************************************************************/

int as_incoming_binary (ASHttpServer *server, TCPC *tcpcon, ASPacket *request)
{
	assert (AS->upman);
	as_upman_start_binary (AS->upman, tcpcon, request);

	return TRUE; /* as_upman_start_binary always handles connection and
	              * request */
}

/*****************************************************************************/

int as_incoming_push (ASHttpServer *server, TCPC *tcpcon, String *buf)
{
	unsigned char *hex, *nl;
	int hlen;
	ASHash *hash;
	as_uint32 push_id;

	if ((nl = strchr (buf->str, '\n')))
	    *nl = '\0';

#if 0
	AS_HEAVY_DBG_1 ("Got push line: %s", buf->str);
#endif

	/* parse push reply */
	if (strncmp (buf->str, "PUSH SHA1:", 10) ||
	    !(hex = as_hex_decode (buf->str+10, &hlen)))
	{
		AS_ERR_2 ("malformed push from %s: '%s'",
		          net_ip_str(tcpcon->host), buf->str);

		return FALSE;
	}

	if (hlen < 24)
	{
		AS_ERR_2 ("truncated push from %s: '%s'",
		          net_ip_str(tcpcon->host), buf->str);
		free (hex);

		return FALSE;
	}
		
	/* Create hash. */
	if (!(hash = as_hash_create (hex, 20)))
	{
		free (hex);
		return FALSE;
	}

	/* Get push id. See as_push.c for how we send it. */
	push_id = (((as_uint32)hex[20]) << 24) | 
	          (((as_uint32)hex[21]) << 16) |
	          (((as_uint32)hex[22]) << 8) |
	          (((as_uint32)hex[23]));

	free (hex);

	AS_HEAVY_DBG_3 ("Got push %d from %s for '%s'", (unsigned int)push_id,
	                net_ip_str(tcpcon->host), as_hash_str (hash));

	/* Let push manager handle it */
	if (!as_pushman_accept (AS->pushman, hash, push_id, tcpcon))
	{
		as_hash_free (hash);
		return FALSE; /* Close connection */
	}

	as_hash_free (hash);

	return TRUE; /* Don't close connection */
}

/*****************************************************************************/
