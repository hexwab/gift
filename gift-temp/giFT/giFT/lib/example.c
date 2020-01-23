/*
 * $Id: example.c,v 1.12 2003/03/19 11:10:08 jasta Exp $
 *
 * Build with gcc -o example example.c -g -Wall -lgiFT
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

#include <giFT/gift.h>

#include <giFT/event.h>
#include <giFT/tcpc.h>

/*****************************************************************************/

static char          *search_host  = NULL;
static in_port_t      search_port  = 1213;
static char          *search_query = NULL;

/*****************************************************************************/

static void bail_out (TCPC *c)
{
	/*
	 * Close this socket connection and free the associated data.  We then
	 * need to bail out of the event loop as we no longer have any reason to
	 * still be running.
	 */
	tcp_close (c);
	event_quit (0);
}

static void dump_meta (Interface *p, InterfaceNode *node, void *udata)
{
	printf ("\t\t%s = %s\n", node->key, node->value);
}

static void handle_search_result (Interface *cmd)
{
	/*
	 * Look for an empty ITEM; tag to terminate this search.
	 */
	if (!interface_get (cmd, "url"))
	{
		printf (" -- END OF SEARCH --\n");
		return;
	}

	/*
	 * Format the search result to prove that we have cleanly parsed the
	 * interface protocol data.
	 */
	printf ("[%lu] hit: %s\n",
	        INTERFACE_GETLU (cmd, "availability"),
	        interface_get (cmd, "url"));
	printf ("\tuser: %s\n", interface_get (cmd, "user"));

	/*
	 * Display the meta data with a loop on all meta children.
	 */
	printf ("\tmeta:\n");
	interface_foreach (cmd, "meta", (InterfaceForeach)dump_meta, NULL);
}

static void conn_read (int fd, input_id id, TCPC *c)
{
	Interface      *cmd;
	unsigned char *data;
	size_t         data_len;
	FDBuf         *buf;
	int            n;

	/*
	 * Grab the read buffer associated with this connection.
	 */
	buf = tcp_readbuf (c);

	/*
	 * Read as much data as possible, attempting to locate a ';' on the
	 * data stream.
	 */
	if ((n = fdbuf_delim (buf, ";")) < 0)
	{
		bail_out (c);
		return;
	}

	/*
	 * We were unable to locate the sentinel character (';') so we bail out
	 * of this call and wait for more data through the event loop.
	 */
	if (n > 0)
		return;

	/*
	 * Parse the interface protocol into a usable high-level structure for
	 * handling.
	 */
	data = fdbuf_data (buf, &data_len);
	assert (data != NULL);

	/*
	 * Parse the packet into a tree-structure accessable through the API
	 * provided by interface.c.  Then process the object with our high-level
	 * logic and cleanup.
	 */
	cmd = interface_unserialize ((char *)data, data_len);
	handle_search_result (cmd);
	interface_free (cmd);

	/*
	 * Release the buffer so that the subsequent calls can use a fresh
	 * buffer.
	 */
	fdbuf_release (buf);
}

static void send_search (TCPC *c)
{
	Interface *cmd;

	/*
	 * Construct a new "search" interface command.
	 */
	if (!(cmd = interface_new ("search", NULL)))
		return;

	/*
	 * Add the query member.
	 */
	interface_put (cmd, "query", search_query);

	/*
	 * Serialize the command and write over the socket.  We must also manually
	 * clean up the memory used by cmd here as well.
	 */
	interface_send (cmd, c);
	interface_free (cmd);
}

static void conn_establish (int fd, input_id id, TCPC *c)
{
	/*
	 * Remove the connection establish handler, even if we have succeeded
	 * we will want to switch over to the main read handler.
	 */
	input_remove (id);

	/*
	 * Check the socket for errors before proceeding.  If errors occur we can
	 * safely bail out now that the input has been removed.
	 */
	if (net_sock_error (c->fd))
	{
		log_error ("unable to connect to %s:%hu: %s",
		           net_ip_str (c->host), c->port, platform_error ());
		bail_out (c);
		return;
	}

	/*
	 * No errors found on this socket and it is now waiting for writes.  Push
	 * the search request.
	 */
	send_search (c);

	/*
	 * Switch this input handler over to the main packet reading loop.
	 */
	input_add (c->fd, c, INPUT_READ, (InputCallback)conn_read, TIMEOUT_DEF)
}

static int conn_gift (char *host, in_port_t port)
{
	TCPC *c;

	/*
	 * Make an outgoing, non-blocking connection to host:port.  Since this
	 * connection is not blocking we will need to assign an input to callback
	 * to us when the connection has completed (or failed).
	 */
	if (!(c = tcp_open (net_ip (host), port, FALSE)))
		return FALSE;

	/*
	 * Register the input with a default input timer.
	 */
	input_add (c->fd, c, INPUT_WRITE, (InputCallback)conn_establish,
	           TIMEOUT_DEF);
	return TRUE;
}

/*****************************************************************************/

static void usage (char *prog)
{
	printf ("Usage: %s <host> <query>\n"
			"\n"
			"Host is specified in the standard dots-and-numbers notiation.\n"
			"Query is the search query you wish to perform.\n",
			prog);
}

int main (int argc, char **argv)
{
	if (argc < 3)
	{
		usage (argv[0]);
		return 1;
	}

	search_host  = argv[1];
	search_port  = 1213;
	search_query = argv[2];

	/*
	 * Initialize the logging subsystem.
	 */
	log_init (GLOG_STDOUT, "gsh", 0, 0, NULL);

	/*
	 * Initialize the input and timer tables.  This must be called prior to
	 * {input,timer}_add.
	 */
	event_init ();

	/*
	 * Begin the outgoing connection to the giFT daemon at HOST:PORT.
	 */
	if (!conn_gift (search_host, search_port))
		return 1;

	/*
	 * Start the main event loop.
	 */
	event_loop ();

	return 0;
}
