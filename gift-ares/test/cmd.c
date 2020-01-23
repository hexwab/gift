/*
 * $Id: cmd.c,v 1.40 2005/12/02 18:57:24 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "cmd.h"
#include "as_ares.h"

/*****************************************************************************/

#define COMMAND_FUNC(func) static as_bool command_##func (int argc, char *argv[])
#define COMMAND(func,param_str,descr) { #func, param_str, descr, command_##func },
#define COMMAND_NULL { NULL, NULL, NULL, NULL }

COMMAND_FUNC (help);

COMMAND_FUNC (event_test);

COMMAND_FUNC (load_nodes);
COMMAND_FUNC (save_nodes);
COMMAND_FUNC (connect);
COMMAND_FUNC (connect_to);
COMMAND_FUNC (go);
COMMAND_FUNC (search);
COMMAND_FUNC (locate);
COMMAND_FUNC (info);
COMMAND_FUNC (result_stats);
COMMAND_FUNC (clear);
COMMAND_FUNC (dl);
COMMAND_FUNC (dl_resume);
COMMAND_FUNC (dl_list);
COMMAND_FUNC (dl_find_more);
COMMAND_FUNC (dl_pause);
COMMAND_FUNC (dl_unpause);
COMMAND_FUNC (dl_cancel);
COMMAND_FUNC (share);
COMMAND_FUNC (share_stats);
COMMAND_FUNC (network_stats);
COMMAND_FUNC (upload_stats);
COMMAND_FUNC (exec);

COMMAND_FUNC (quit);

static struct command_t
{
	char *name;
	char *param_str;
	char *descr;
	as_bool (*func)(int argc, char *argv[]);
}
commands[] =
{
	COMMAND (help,
	         "",
	         "Display list of commands.")

	COMMAND (event_test,
	         "",
	         "Test event system.")

	COMMAND (load_nodes,
	         "<file>",
	         "Load nodes file.")
	COMMAND (save_nodes,
	         "<file>",
	         "Save nodes file.")

	COMMAND (connect,
	         "<no_sessions>",
	         "Maintain no_sessions connections to network.")

	COMMAND (connect_to,
	         "<host> [<port>]",
	         "Create session to a given host.")

	COMMAND (go,
	         "",
	         "Loads nodes and connects to network.")

	COMMAND (search,
	         "<query>",
	         "Search connected hosts for files.")

	COMMAND (locate,
	         "<base64 hash>",
	         "Search sources for file.")

	COMMAND (info,
	         "<result number>",
	         "Show details for a given search result.")

	COMMAND (result_stats,
	         "",
	         "Show stats about search results.")

	COMMAND (clear,
	         "",
	         "Clear search results.")

	COMMAND (dl,
	         "<result number>",
	         "Download search result.")

	COMMAND (dl_resume,
	         "<file>",
	         "Resume incomplete download.")

	COMMAND (dl_list,
	         "",
			 "List downloads.")

	COMMAND (dl_find_more,
	         "<download id>",
	         "Find more sources for download.")

	COMMAND (dl_pause,
	         "<download id>",
	         "Pause download.")

	COMMAND (dl_unpause,
	         "<download id>",
	         "Resume paused download.")

	COMMAND (dl_cancel,
	         "<download id>",
	         "Cancel download.")

	COMMAND (share,
	         "<path> <size> <realm> <hash> [<metadata pairs>...]",
	         "Share file.")

	COMMAND (share_stats,
	         "",
	         "Show stats about shares.")

	COMMAND (network_stats,
	         "",
	         "Show stats about the network.")

	COMMAND (upload_stats,
	         "",
	         "Show stats about uploads.")

	COMMAND (exec,
		 "<file>",
		 "Read commands from file.")

	COMMAND (quit,
             "",
             "Quit this application.")

	COMMAND_NULL
};


/*****************************************************************************/

void print_cmd_usage (struct command_t *cmd)
{
	printf ("Command \"%s\" failed. Usage:\n", cmd->name);
	printf ("  %s %s\n", cmd->name, cmd->param_str);
}

as_bool dispatch_cmd (int argc, char *argv[])
{
	struct command_t *cmd;

	if (argc < 1)
		return FALSE;

	/* find handler and call it */
	for (cmd = commands; cmd->name; cmd++)
		if (!strcmp (cmd->name, argv[0]))
		{
			if (cmd->func (argc, argv))
				return TRUE;

			print_cmd_usage (cmd);
			return FALSE;
		}

	printf ("Unknown command \"%s\", try \"help\"\n", argv[0]);

	return FALSE;
}

/*****************************************************************************/

COMMAND_FUNC (help)
{
	struct command_t *cmd;
	
	printf ("Available commands:\n\n");

	for (cmd = commands; cmd->name; cmd++)
	{
			printf ("* %s %s\n", cmd->name, cmd->param_str);
			printf ("      %s\n", cmd->descr);
	}	

	return TRUE;
}

/*****************************************************************************/

COMMAND_FUNC (event_test)
{
	test_event_system ();
	return TRUE;
}

/*****************************************************************************/

COMMAND_FUNC (load_nodes)
{
	if (argc != 2)
		return FALSE;

	printf ("Loading nodes from file %s.\n", argv[1]);

	if (!as_nodeman_load (AS->nodeman, argv[1]))
		printf ("Node file load failed.\n");

	return TRUE;
}

COMMAND_FUNC (save_nodes)
{
	if (argc != 2)
		return FALSE;

	printf ("Saving nodes to file %s.\n", argv[1]);

	if (!as_nodeman_save (AS->nodeman, argv[1]))
		printf ("Node file save failed.\n");

	return TRUE;
}

COMMAND_FUNC (connect)
{
	int i;

	if (argc != 2)
		return FALSE;

	i = atoi (argv[1]);
	assert (i >= 0);

	printf ("Telling session manager to connect to %d nodes.\n", i);

	as_sessman_connect (AS->sessman, i);

	return TRUE;
}

COMMAND_FUNC (connect_to)
{
	ASSession *sess;
	in_addr_t ip;
	in_port_t port;

	if (argc < 2)
		return FALSE;

	ip = net_ip (argv[1]);
	
	/* if no port is given derive it from ip */
	if (argc > 2)
		port = atoi (argv[2]);
	else
		port = as_ip2port (ip);

	printf ("connecting to %s (%08x), port %d\n", argv[1], ip, port);
	sess = as_session_create (NULL, NULL);
	assert (sess);
	as_session_connect (sess, ip, port);

	return TRUE;
}

COMMAND_FUNC (go)
{
	static char *load[]    = { "load_nodes", "nodes" };
	static char *connect[] = { "connect", "4" };
	
	dispatch_cmd (sizeof (load) / sizeof (char*), load);
	dispatch_cmd (sizeof (connect) / sizeof (char*), connect);

	return TRUE;
}

/*****************************************************************************/

static const char *realm_chars="RA?S?VDI";

/* warning: ludicrously inefficient list code ahead */
static List *results = NULL;
static ASSearch *test_search = NULL;

static as_bool meta_tag_itr (ASMetaTag *tag, void *udata)
{
	printf ("  %s: %s\n", tag->name, tag->value);
	return TRUE;
}

static void search_callback (ASSearch *search, ASResult *r, as_bool duplicate)
{
	assert (search == test_search);

#if 1
	/* usable */
	printf ("%3d) %25s %10d %c [%s]\n", list_length (results),
		r->source->username, r->filesize,
		realm_chars[r->realm], r->filename);
#else
	/* debuggable */
	printf ("%3d) %02x %02x %02x (%d) %02x %02x %02x  %s %s\n", list_length (results),
		r->unknown, r->unk[0], r->unk[1],
		r->unk[0] | ((int)r->unk[1] << 8),
		r->unk[2], r->unk[3], r->unk[4],
		net_ip_str (r->source->host), r->source->username);
#endif

#if 0
	{
	int i;

	printf ("Meta tags:\n");
	i = as_meta_foreach_tag (r->meta, meta_tag_itr, NULL);
	printf ("(%d tags total)\n", i);
	}
#endif

	results = list_append (results, r);
}

COMMAND_FUNC (search)
{
	unsigned char *query;

	if (argc < 2)
		return FALSE;

	if (test_search)
	{
		printf ("Only one search allowed at a time in this test app\n");
		return TRUE;
	}

	assert (results == NULL);

	query = argv[1];

	test_search = as_searchman_search (AS->searchman,
	                                   (ASSearchResultCb) search_callback,
	                                   query, SEARCH_ANY);

	if (!test_search)
	{
		printf ("Failed to start search for \"%s\"\n", query);
		return TRUE;
	}
	
	printf ("Started search for \"%s\"\n", query);

	return TRUE;
}

COMMAND_FUNC (locate)
{
	ASHash *hash;

	if (argc < 2)
		return FALSE;

	if (test_search)
	{
		printf ("Only one search allowed at a time in this test app\n");
		return TRUE;
	}

	assert (results == NULL);

	if (!(hash = as_hash_decode (argv[1])))
	{
		printf ("Couldn't parse hash %s\n", argv[1]);
		return TRUE;
	}

	test_search = as_searchman_locate (AS->searchman,
	                                   (ASSearchResultCb) search_callback,
	                                   hash);

	if (!test_search)
	{
		printf ("Failed to start locate for \"%s\"\n", as_hash_str (hash));
		as_hash_free (hash);
		return TRUE;
	}
	
	printf ("Started locate for \"%s\"\n", as_hash_str (hash));

	as_hash_free (hash);
	return TRUE;
}

COMMAND_FUNC (info)
{
	int rnum;
	int i;
	char *str;
	ASResult *r;

	if (argc < 2)
		return FALSE;

	rnum = atoi (argv[1]);

	r = list_nth_data (results, rnum);

	if (!r)
	{
		printf ("Invalid result number\n");
		return TRUE;
	}

	printf ("Filename: %s (extension '%s')\n", r->filename, r->fileext);
	printf ("Filesize: %d bytes\n", r->filesize);
	printf ("User: %s (%s:%d)\n", r->source->username,
		net_ip_str (r->source->host), r->source->port);

	printf ("SHA1: ");
	for (i=0; i < AS_HASH_SIZE; i++)
		printf ("%02x", r->hash->data[i]);
	str = as_hash_encode (r->hash);
	printf (" [%s]\n", str);
	free (str);

	if (r->meta)
	{
		printf ("Meta tags:\n");
		i = as_meta_foreach_tag (r->meta, meta_tag_itr, NULL);
		printf ("(%d tags total)\n", i);
	}

	return TRUE;
}

COMMAND_FUNC (result_stats)
{
	ASResult *r;
	List *l;
	int result_count = 0, firewalled = 0, push_info = 0;

	if (!results)
	{
		printf ("No results.\n");
		return FALSE;
	}

	for (l = results; l; l = l->next)
	{
		r = l->data;
		result_count++;

		if (as_source_firewalled (r->source))
			firewalled++;

		if (as_source_has_push_info (r->source))
			push_info++;
	}

	printf ("Results: %d\n", result_count);
	printf ("Firewalled: %d (%d%%)\n", firewalled,
	        firewalled == 0 ? 0 : result_count * 100 / firewalled);
	/* push info is probably not needed at all if pushes are triggerd by hash
	 * searches 
	 */
	printf ("Have push info: %d (%d%%)\n", push_info,
	        push_info == 0 ? 0 : result_count * 100 / push_info);

	return TRUE;
}

COMMAND_FUNC (clear)
{
	if (!test_search)
	{
		printf ("No search to clear.\n");
		return TRUE;
	}
	
	results = list_free (results);

	as_searchman_remove (AS->searchman, test_search);
	test_search = NULL;

	printf ("Cleared search results.\n");

	return TRUE;
}

/*****************************************************************************/

as_bool downman_cb (ASDownMan *man, ASDownload *dl, ASDownloadState state)
{
	printf ("Download [%s]: %s\n", as_download_state_str (dl), dl->filename);

	switch (state)
	{
	case DOWNLOAD_INVALID:
	case DOWNLOAD_NEW:
	case DOWNLOAD_ACTIVE:
	case DOWNLOAD_QUEUED:
	case DOWNLOAD_PAUSED:
	case DOWNLOAD_VERIFYING:
		break;
	case DOWNLOAD_CANCELLED:
	case DOWNLOAD_COMPLETE:
	case DOWNLOAD_FAILED:
		/* remove download */
		printf ("Removing finished download\n");
		if (!as_downman_remove (man, dl))
			printf ("Error: Couldn't remove download\n");
		break;
	}

	return TRUE;
}

COMMAND_FUNC (dl)
{
	int rnum;
	ASResult *r;
	ASDownload *dl;

	if (argc < 2)
		return FALSE;

	rnum = atoi (argv[1]);

	if (!(r = list_nth_data (results, rnum)))
	{
		printf ("Invalid result number\n");
		return TRUE;
	}
	
	/* make sure callback is set */
	as_downman_set_state_cb (AS->downman, downman_cb);

	if (!(dl = as_downman_start_result (AS->downman, r, r->filename)))
	{
		printf ("Download start failed\n");
		return TRUE;
	}

	printf ("Download [%p] of \"%s\" started\n", dl, r->filename);

	return TRUE;
}

COMMAND_FUNC (dl_resume)
{
	ASDownload *dl;
	char *filename;

	if (argc < 2)
		return FALSE;

	filename = argv[1];

	/* make sure callback is set */
	as_downman_set_state_cb (AS->downman, downman_cb);

	/* restart download */
	if (!(dl = as_downman_restart_file (AS->downman, filename)))
	{
		printf ("Download restart failed\n");
		return TRUE;
	}

	printf ("Download of \"%s\" restarted\n", dl->filename);

	return TRUE;
}

COMMAND_FUNC (dl_list)
{
	List *l;
	ASDownload *dl;
	int i = 0;

	printf ("Active downloads:\n");
	printf ("  Id         State      Complete  File\n");

	for (l = AS->downman->downloads; l; l = l->next)
	{
		dl = l->data;
		printf ("  [%p] %-10s     %3d%%  %s\n", dl,
		        as_download_state_str (dl),
		        dl->received * 100 / dl->size, dl->filename);
		i++;
	}

	printf ("(%d downloads)\n", i);
	
	return TRUE;
}

COMMAND_FUNC (dl_find_more)
{
	ASDownload *dl;

	if (argc < 2)
		return FALSE;

	if (sscanf (argv[1], "%p", &dl) != 1)
	{
		printf ("Couldn't parse Id\n");
		return TRUE;
	}

	if (!as_downman_find_sources (AS->downman, dl))
	{
		printf ("Find sources failed\n");
		return TRUE;
	}

	printf ("Started source search for download \"%s\"\n", dl->filename);

	return TRUE;
}

COMMAND_FUNC (dl_pause)
{
	ASDownload *dl;

	if (argc < 2)
		return FALSE;

	if (sscanf (argv[1], "%p", &dl) != 1)
	{
		printf ("Couldn't parse Id\n");
		return TRUE;
	}

	if (!as_downman_pause (AS->downman, dl, TRUE))
	{
		printf ("Pause download failed\n");
		return TRUE;
	}

	printf ("Paused download \"%s\"\n", dl->filename);

	return TRUE;
}

COMMAND_FUNC (dl_unpause)
{
	ASDownload *dl;

	if (argc < 2)
		return FALSE;

	if (sscanf (argv[1], "%p", &dl) != 1)
	{
		printf ("Couldn't parse Id\n");
		return TRUE;
	}

	if (!as_downman_pause (AS->downman, dl, FALSE))
	{
		printf ("Resume download failed\n");
		return TRUE;
	}

	printf ("Resumed paused download \"%s\"\n", dl->filename);

	return TRUE;
}

COMMAND_FUNC (dl_cancel)
{
	ASDownload *dl;

	if (argc < 2)
		return FALSE;

	if (sscanf (argv[1], "%p", &dl) != 1)
	{
		printf ("Couldn't parse Id\n");
		return TRUE;
	}

	if (!as_downman_cancel (AS->downman, dl))
	{
		printf ("Cancelling download failed\n");
		return TRUE;
	}

	printf ("Cancelled download \"%s\"\n", dl->filename);

	if (!as_downman_remove (AS->downman, dl))
		printf ("Error: Couldn't remove download\n");

	printf ("Removed cancelled download\n");

	return TRUE;
}

/*****************************************************************************/

COMMAND_FUNC (share)
{
	char *path;
	int size;
	ASRealm realm;
	ASHash *hash;
	ASMeta *meta;
	ASShare *share;
	int i;

	if (argc < 5)
		return FALSE;

	if (!(argc & 1))
		return FALSE;

	path = argv[1];
	size = atoi(argv[2]);
	realm = atoi(argv[3]);
	hash = as_hash_decode (argv[4]);
	meta = as_meta_create ();

	if (!meta)
		return FALSE;

	for (i = 5; i < argc - 1; i += 2)
	{
		char *name = argv[i], *value = argv[i+1];

		as_meta_add_tag (meta, name, value);
	}

	share = as_share_create (path, hash, meta, size, realm);
	
	if (!share)
	{
		as_meta_free (meta);
		return FALSE;
	}
	
	if (!as_shareman_add (AS->shareman, share))
	{
		as_share_free (share);
		return FALSE;
	}

	return TRUE;
}

COMMAND_FUNC (share_stats)
{
	printf ("%d files shared, %.2f Mb\n",
		AS->shareman->nshares, AS->shareman->size);

	return TRUE;
}

COMMAND_FUNC (network_stats)
{
	printf ("Connected to %u supernodes. %u users online\n",
		list_length (AS->sessman->connected), AS->netinfo->users);
	printf ("%u total files, %u Gb\n",
		AS->netinfo->files, AS->netinfo->size);

	return TRUE;
}

COMMAND_FUNC (upload_stats)
{
	printf ("%u uploads (%u max), %u queued\n",
	        AS->upman->nuploads, AS_CONF_INT (AS_UPLOAD_MAX_ACTIVE),
	        AS->upman->nqueued);

	return TRUE;
}

COMMAND_FUNC (exec)
{
	FILE *f;
	int count = 0;
	int eargc;
	char **eargv;
	char buf[16384];
	if (argc < 2)
		return FALSE;

	if (!(f = fopen (argv[1], "r")))
		return FALSE;

	while (fgets (buf, sizeof(buf), f))
	{
		int len = strlen(buf);
		if (len && buf[len-1]=='\n')
		{
			char *copy;
			buf[len-1] = 0;
			copy = strdup(buf);
			parse_argv (buf, &eargc, &eargv);
			if (eargc >  0)
			{
				int ret = dispatch_cmd (eargc, eargv);
				
				if (!ret)
				{				
					printf ("Command was: '%s'\nAborting.\n", copy);
					free (copy);
					free (eargv);
					break;
				}
			}				
			count++;
			free (copy);
			free (eargv);
		}
	}

	fclose (f);
	printf ("%d commands processed.\n", count);

	return TRUE;
}

/*****************************************************************************/

COMMAND_FUNC (quit)
{
	/* FIXME: cleanup properly */
	
	printf ("Terminating event loop...\n");
	as_event_quit ();
	return TRUE;
}

/*****************************************************************************/

