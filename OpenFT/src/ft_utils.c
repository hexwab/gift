/*
 * $Id: ft_utils.c,v 1.21 2003/11/21 16:05:18 jasta Exp $
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

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>

#include "ft_openft.h"

#include "ft_netorg.h"

/*****************************************************************************/

static void accept_test_result (TCPC *c, TCPC *verify, int success)
{
	/* what the hell? */
	if (!c)
		return;

	/* TODO - notify this connection that it's configured port has been
	 * changed */

	/* one of the verification sockets failed, those bastards */
	if (!success && FT_NODE_INFO(c)->port_openft != 0)
	{
		/* set ip as 0 (firewalled) */
		ft_node_set_port (FT_NODE(c), 0);
		ft_node_set_http_port (FT_NODE(c), 0);
	}

	if (verify)
	{
		if (verify == FT_SESSION(c)->verify_ft)
			FT_SESSION(c)->verify_ft = NULL;
		else if (verify == FT_SESSION(c)->verify_http)
			FT_SESSION(c)->verify_http = NULL;

		tcp_close (verify);
	}

	/* do not set verified to TRUE until both call-back connections return */
	if (FT_SESSION(c)->verify_ft || FT_SESSION(c)->verify_http)
		return;

	FT_SESSION(c)->verified = TRUE;
	ft_session_stage (c, 2);
}

static void accept_test_verify (int fd, input_id id, TCPC *verify)
{
	TCPC *c;

	c = verify->udata;
	assert (c != NULL);

	/* they lied, we can't connect to them */
	if (net_sock_error (verify->fd))
	{
		accept_test_result (c, verify, FALSE);
		return;
	}

	accept_test_result (c, verify, TRUE);
}

static void accept_test_port (TCPC *c, in_port_t port,
                              TCPC **verify)
{
	if ((*verify))
		tcp_close (*verify);

	if (((*verify) = tcp_open (FT_NODE_INFO(c)->host, port, FALSE)))
		(*verify)->udata = c;

	/* unable to perform the verification, this could be a very big problem as
	 * it's likely not the fault of the remote node */
	if (!(*verify) || (*verify)->fd <= 0)
	{
		FT->DBGFN (FT, "%s:%hu: %s", net_ip_str (c->host), port,
		           GIFT_NETERROR());
		accept_test_result (c, *verify, FALSE);
		return;
	}

	/* let select handle the rest of the verification */
	input_add ((*verify)->fd, *verify, INPUT_WRITE,
	           (InputCallback)accept_test_verify, TIMEOUT_DEF);
}

/* connect to the given node in order to verify their port configuration is
 * accurate */
void ft_accept_test (TCPC *c)
{
	assert (FT_SESSION(c)->verified == FALSE);

	if (FT_NODE_INFO(c)->host == 0 || FT_NODE_INFO(c)->port_openft == 0)
	{
		accept_test_result (c, NULL, FALSE);
		return;
	}

	/* if either of these fail, the same result (accept_test_result) will
	 * occur */
	accept_test_port (c, FT_NODE_INFO(c)->port_openft, &(FT_SESSION(c)->verify_ft));
	accept_test_port (c, FT_NODE_INFO(c)->port_openft, &(FT_SESSION(c)->verify_http));
}
