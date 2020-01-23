/*
 * ft_utils.c
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

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>

#include "ft_openft.h"

#include "ft_netorg.h"

#include "ft_xfer.h"

#include "log.h"

/*****************************************************************************/

/* maximum parents to submit to */
#define MAX_PARENTS config_get_int (OPENFT->conf, "sharing/parents=3")

/*****************************************************************************/

static void accept_test_result (Connection *c, Connection *verify, int success)
{
	/* what the hell? */
	if (!c)
		return;

	/* TODO - notify this connection that it's configured port has been
	 * changed */

	/* one of the verification sockets failed, those bastards */
	if (!success && FT_NODE(c)->port != 0)
	{
		/* set ip as 0 (firewalled) */
		ft_node_set_port (FT_NODE(c), 0);
		ft_node_set_http_port (FT_NODE(c), 0);
	}

	if (verify)
	{
		if (verify == FT_SESSION(c)->verify_openft)
			FT_SESSION(c)->verify_openft = NULL;
		else if (verify == FT_SESSION(c)->verify_http)
			FT_SESSION(c)->verify_http = NULL;

		connection_close (verify);
	}

	/* do not set verified to TRUE until both call-back connections return */
	if (FT_SESSION(c)->verify_openft || FT_SESSION(c)->verify_http)
		return;

	FT_SESSION(c)->verified = TRUE;

	/* set the host share element appropriately, so that searches may be
	 * effectively returned */
	ft_shost_verified (FT_NODE(c)->ip, FT_NODE(c)->port, FT_NODE(c)->http_port,
	                   TRUE);

	ft_session_stage (c, 2);
}

static void accept_test_verify (Protocol *p, Connection *verify)
{
	Connection *c;

	c = verify->data;

	/* they lied.  we can't connect to them */
	if (net_sock_error (verify->fd))
	{
		accept_test_result (c, verify, FALSE);
		return;
	}

	accept_test_result (c, verify, TRUE);
}

static void accept_test_port (Connection *c, unsigned short port,
                              Connection **verify)
{
	if (!(*verify))
		*verify = connection_new (openft_p);
	else
	{
		input_remove (*verify);
		net_close ((*verify)->fd);
	}

	(*verify)->fd   = net_connect (net_ip_str (FT_NODE(c)->ip), port, FALSE);
	(*verify)->data = c; /* set parent */

	/* TODO - if this happens self is likely broken, not the incoming
	 * connection. what to do, what to do */
	if ((*verify)->fd <= 0)
	{
		accept_test_result (c, *verify, FALSE);
		return;
	}

	/* let select handle the rest of the verification */
	input_add (openft_p, *verify, INPUT_WRITE,
	           (InputCallback)accept_test_verify, TRUE);
}

/* connect to the given node in order to verify their port configuration is
 * accurate */
void ft_accept_test (Connection *c)
{
	if (FT_SESSION(c)->verified)
	{
		TRACE_SOCK (("connection already verified"));
		return;
	}

	if (!FT_NODE(c)->ip || FT_NODE(c)->port == 0)
	{
		accept_test_result (c, NULL, FALSE);
		return;
	}

	/* if either of these fail, the same result (accept_test_result) will
	 * occur */
	accept_test_port (c, FT_NODE(c)->port, &(FT_SESSION(c)->verify_openft));
	accept_test_port (c, FT_NODE(c)->http_port, &(FT_SESSION(c)->verify_http));
}

/*****************************************************************************/

int validate_share_submit ()
{
	int parents;

	parents = ft_netorg_length (NODE_PARENT, NODE_CONNECTED);

	return (parents < MAX_PARENTS);
}

int parents_needed ()
{
	int parents;

	parents = ft_netorg_length (NODE_PARENT, NODE_CONNECTED);

	return MAX (0, (MAX_PARENTS - parents));
}

/*****************************************************************************/

char *node_class_str (unsigned short klass)
{
	char *str;

	if (klass & NODE_INDEX)
		str = "INDEX";
	else if (klass & NODE_PARENT)
		str = "PARENT";
	else if (klass & NODE_SEARCH)
		str = "SEARCH";
	else if (klass & NODE_CHILD)
		str = "CHILD";
	else if (klass & NODE_USER)
		str = "USER";
	else
		str = "NONE";

	return str;
}
