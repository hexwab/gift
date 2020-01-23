/*
 * $Id: main.c,v 1.5 2006/02/27 01:50:35 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"
#include "main.h"

/*****************************************************************************/

/*
 * Reads nodes from sources file and connects to them all. If connection is
 * successful ip is added to new file and all ips from we get from that node
 * are added as well.
 */

/*****************************************************************************/

List *org_nodes = NULL;
List *new_nodes = NULL;
int nnew_nodes = 0;
int max_nodes = 2000;
int max_parallel = 100;
int parallel = 0;
int ndown = 0;
int nloaded = 0;
const char *save_filename = NULL;
as_bool no_new_nodes = FALSE;
List *sessions = NULL;

static void crawl_next();

/*****************************************************************************/

static void add_node (in_addr_t host, in_port_t port)
{
	List *link;
	ASNode *node;

	/* already have this node? FIXME: optimize with hashtable */
	for (link = new_nodes; link; link = link->next)
	{
		node = (ASNode *) link->data;
		assert (node);

		if (node->host == host && node->port == port)
			return;
	}

	node = as_node_create (host, port);
	assert (node);
	new_nodes = list_prepend (new_nodes, node);
	nnew_nodes++;

	AS_DBG_1 ("CRAWLER: Added supernode %s", net_ip_str (host));
}

/* Called for state changes. Return FALSE if the session has been freed and
 * can no longer be used after the callback.
 */
static as_bool session_state_cb (ASSession *session, ASSessionState new_state)
{
	List *link;
	ASNode *node;

	switch (new_state)
	{
	case SESSION_CONNECTING:
		AS_DBG_1 ("CRAWLER: Connecting %s", net_ip_str (session->host));
		return TRUE;
	case SESSION_HANDSHAKING:
		AS_DBG_1 ("CRAWLER: Handshaking %s", net_ip_str (session->host));
		return TRUE;
	case SESSION_CONNECTED:
		AS_DBG_1 ("CRAWLER: Connected %s", net_ip_str (session->host));
	
		/* save this node */
		add_node (session->host, session->port);

		/* save nodes added to AS->nodeman during handshake */
		if (!no_new_nodes)
		{
			for (link = AS->nodeman->nodes; link; link = link->next)
			{
				node = (ASNode *) link->data;
				assert (node);
				add_node (node->host, node->port);
			}
		}
		
		/* empty for next round */
		as_nodeman_empty (AS->nodeman);

		/* move to next supernode */
		parallel--;
		sessions = list_remove (sessions, session);
		AS_HEAVY_DBG_2 ("CRAWLER: parallel now %d (%d) after connected decrement",
		                parallel, list_length (sessions));
		assert (parallel >= 0);
		assert (parallel == list_length (sessions));
		crawl_next ();
		as_session_free (session);
		return FALSE;

	case SESSION_FAILED:
		ndown++;
		/* fall through */
	case SESSION_DISCONNECTED:
		AS_DBG_1 ("CRAWLER: Failed %s", net_ip_str (session->host));
		parallel--;
		sessions = list_remove (sessions, session);
		AS_HEAVY_DBG_2 ("CRAWLER: parallel now %d (%d) after failure decrement",
		                parallel, list_length (sessions));
		assert (parallel >= 0);
		assert (parallel == list_length (sessions));
		crawl_next ();
		as_session_free (session);
		return FALSE;
	default:
		abort ();
	};

	abort ();
}

/*****************************************************************************/

static void crawl_next()
{
	ASSession *sess;
	ASNode *node;
	int saved = 0;
	FILE *fp;

	while (org_nodes && parallel < max_parallel && nnew_nodes < max_nodes)
	{
		node = (ASNode *) org_nodes->data;
		assert (node);
		org_nodes = list_remove_link (org_nodes, org_nodes);

		sess = as_session_create (session_state_cb, NULL);
		assert (sess);

		if (!as_session_connect (sess, node->host, node->port))
		{
			as_session_free (sess);
			as_node_free (node);
			continue;
		}

		parallel++;
		sessions = list_prepend (sessions, sess);
		as_node_free (node);
	}

	AS_HEAVY_DBG_2 ("CRAWLER: parallel now %d (%d) after new connecting",
	                parallel, list_length (sessions));
	assert (parallel >= 0);
	assert (parallel == list_length (sessions));

	if ((!org_nodes || nnew_nodes >= max_nodes) && parallel == 0)
	{
		if (!new_nodes)
		{
			/* no new nodes found */
			AS_DBG ("CRAWLER: No nodes were crawled successfully. Exiting with error level 1.");
			exit (1);
		}

		/* we are finished. save new nodes. */
		if (!(fp = fopen (save_filename, "wb")))
		{
			printf ("Couldn't save nodes to file '%s'\n", save_filename);
			exit (1);
		}

		while (new_nodes && saved < max_nodes)
		{
			node = (ASNode *) new_nodes->data;
			assert (node);
	
			fprintf (fp, "%s %u\r\n", net_ip_str (node->host),
									(unsigned int)node->port);

			as_node_free (node);
			new_nodes = list_remove_link (new_nodes, new_nodes);
			nnew_nodes--;
			assert (nnew_nodes >= 0);
			saved++;
		}

		fclose (fp);	
		AS_DBG_2 ("CRAWLER: Saved %d nodes of %d collected",
		          saved, nnew_nodes + saved);
		AS_DBG_2 ("CRAWLER: %d of %d tried source nodes were down", ndown,
		          nloaded - list_length (org_nodes));

		as_event_quit ();
		return;
	}
}

/*****************************************************************************/

int main (int argc, char *argv[])
{
	ASLogger *logger;
	FILE *fp;
	char buf[1024];
	const char *src_filename = NULL;
	int i = 1;

	if (argc < 3 || argc > 5)
	{
		printf ("Usage: crawler [--verify] <src file> <dest file> [max_nodes]\n");
		printf ("Crawls all nodes in src file and writes found nodes to dest file\n");
		printf ("If --verify is specified only the nodes in src file are checked and nodes retrieved from the network are ignored\n");
		printf ("If max_nodes are saved to dest file we stop. Default for max_nodes is 2000.\n");
		exit (1);
	}

	no_new_nodes = FALSE;
	if (gift_strcasecmp (argv[i], "--verify") == 0)
	{
		no_new_nodes = TRUE;
		i++;
	}
	
	src_filename = argv[i++];
	save_filename = argv[i++];

	if (i < argc)
		max_nodes = atoi (argv[i++]);

	/* load nodes */
	if (!(fp = fopen (src_filename, "r")))
	{
		printf ("Couldn't load nodes from file '%s'\n", src_filename);
		exit (1);
	}

	while (fgets (buf, sizeof (buf), fp))
	{
		int port;
		char ip_str[32];
		ASNode *node;
		
		if (strlen (buf) >= sizeof (buf) - 1)
		{
			AS_ERR ("Aborting node file read. Line too long.");
			break;
		}

		if (sscanf (buf, "%31s %u\n", ip_str, &port) != 2)
			continue;

		if (!(node = as_node_create (net_ip (ip_str), (in_port_t)port)))
			continue;

		org_nodes = list_prepend (org_nodes, node);
		nloaded++;
	}

	fclose (fp);
	AS_DBG_1 ("CRAWLER: Loaded %d nodes", nloaded);

	/* make sure save path works before we start crawling */
	if (!(fp = fopen (save_filename, "wb")))
	{
		printf ("Couldn't save nodes to file '%s'\n", save_filename);
		exit (1);
	}
	fclose (fp);

	/* winsock init */
	tcp_startup ();

	/* setup logging */
	logger = as_logger_create ();
	as_logger_add_output (logger, "stderr");
	as_logger_add_output (logger, "ascrawl.log");

	AS_DBG ("Logging subsystem started");

	/* setup event system */
	as_event_init ();

	/* init lib */
	if (!as_init ())
	{
		printf ("FATAL: as_init() failed\n");
		exit (1);
	}

	if (no_new_nodes)
		AS_DBG ("CRAWLER: Only checking nodes already in source file");
	else
		AS_DBG ("CRAWLER: Checking nodes from source file and adding new ones from network");
	AS_DBG_1 ("CRAWLER: Max number of nodes we are going to collect is %d", max_nodes);

	/* start crawl */
	crawl_next ();
	
	/* run event loop */
	AS_DBG ("Entering event loop");
	as_event_loop ();
	AS_DBG ("Left event loop");

	/* cleanup  lib */
	as_cleanup ();

	/* shutdown */
	as_event_shutdown ();
	as_logger_free (logger);

	/* winsock shutdown */
	tcp_cleanup ();

	list_free (org_nodes);
	list_free (new_nodes);

	return 0;
}

/*****************************************************************************/

