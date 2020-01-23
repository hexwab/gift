/*
 * example.c
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

#include <giFT/gift.h>

#include <giFT/network.h>
#include <giFT/event.h>
#include <giFT/nb.h>

/*****************************************************************************/

static char          *search_host  = NULL;
static unsigned short search_port  = 1213;
static char          *search_query = NULL;

/*****************************************************************************/

static void bail_out (Connection *c)
{
	/*
	 * Close this socket connection and free the associated data.  We then
	 * need to bail out of the event loop as we no longer have any reason to
	 * still be running.
	 */
	connection_close (c);
	event_quit (0);
}

static void dump_meta (Interface *p, char *keypath, int children,
                       char *key, char *value, void *udata)
{
	printf ("\t\t%s = %s\n", key, value);
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

static void conn_read (Protocol *p, Connection *c)
{
	Interface *cmd;
	NBRead    *nb;
	int        n;

	/*
	 * Grab the NBRead buffer associated with this connection.
	 */
	nb = nb_active (c->fd);

	/*
	 * Read as much data as possible, attempting to locate a ';' on the
	 * data stream.
	 */
	if ((n = nb_read (nb, 0, ";")) <= 0)
	{
		bail_out (c);
		return;
	}

	/*
	 * We were unable to locate the sentinel character (';') so we bail out
	 * of this call and wait for more data.
	 */
	if (!nb->term)
		return;

	/*
	 * Parse the interface protocol into a usable high-level structure for
	 * handling.
	 */
	cmd = interface_unserialize (nb->data, nb->len);
	handle_search_result (cmd);
	interface_free (cmd);
}

static void send_search (Connection *c)
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

static void conn_establish (Protocol *p, Connection *c)
{
	/*
	 * Remove the connection establish handler, even if we have succeeded
	 * we will want to switch over to the main read handler.
	 */
	input_remove (c);

	/*
	 * Check the socket for errors before proceeding.  If errors occur we can
	 * safely bail out now that the input has been removed.
	 */
	if (net_sock_error (c->fd))
	{
		log_error ("unable to connect to %s:%hu: %s",
		           c->host, c->port, platform_error ());
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
	input_add (NULL, c, INPUT_READ, (InputCallback)conn_read, TRUE);
}

static int conn_gift (char *host, unsigned short port)
{
	Connection *c;

	/*
	 * Make an outgoing, non-blocking connection to host:port.  Since this
	 * connection is not blocking we will need to assign an input to callback
	 * to us when the connection has completed (or failed).
	 */
	if (!(c = connection_open (NULL, host, port, FALSE)))
		return FALSE;

	/*
	 * Register the input with a default input timer.
	 */
	input_add (NULL, c, INPUT_WRITE, (InputCallback)conn_establish, TRUE);
	return TRUE;
}

/*****************************************************************************/

static void usage (char *prog)
{
	printf ("usage: %s <host> <query>\n", prog);
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
