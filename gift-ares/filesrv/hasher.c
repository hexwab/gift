/*
 * $Id: hasher.c,v 1.2 2006/02/01 20:59:15 mkern Exp $
 *
 * Copyright (C) 2005 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2005 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"
#include "hasher.h"

/*****************************************************************************/

/*
 * Traverses directory tree and creates (updates) plain text file with share
 * data of all files.
 */

/*****************************************************************************/

typedef struct
{
	char    *path; /* full path */
	size_t   size;
	time_t   last_modify;
	ASHash  *hash;
} Share;

/*****************************************************************************/

/* returns the total files read plus the old passed as arg 'total' */
static int path_traverse (List **new_shares, int total, char *path);

/* creates ares link from share with ip/port as source */
static char *arlnk_from_share (Share *share, in_addr_t ip, in_port_t port);

/*****************************************************************************/

int main (int argc, char *argv[])
{
	ASLogger *logger;
	FILE *fp;
	char *share_root, *shares_file, *link_file = NULL;
	in_addr_t server_ip = INADDR_NONE;
	in_port_t server_port = 0;
	List *old_shares = NULL, *new_shares = NULL;
	List *lo, *ln;
	char buf[1024*16];
	int nfiles;
	
	if (argc != 4 && argc != 6)
	{
		printf ("Usage: asfilehasher <shares file> <root path> <link file> [<server ip> <server port>]\n");
		printf ("Descends root path and creates shares file.\n");
		printf ("If shares file is not empty files already in it "
		        "will not be hashed again if they haven't changed.\n");
		printf ("If no ip and port are specified the links are generated without sources.\n");
		exit (1);
	}

	shares_file = argv[1];
	share_root = argv[2];
	link_file = argv[3];

	if (argc == 6)
	{
		server_ip = net_ip (argv[4]);
		if (server_ip == 0 || server_ip == INADDR_NONE)
		{
			printf ("Invalid server ip '%s'\n", argv[4]);
			exit (1);
		}

		server_port = atol (argv[5]);
		if (server_port == 0)
		{
			printf ("Invalid server port '%s'\n", argv[5]);
			exit (1);
		}
	}

	/* setup logging */
	logger = as_logger_create ();
	as_logger_add_output (logger, "stdout");
	as_logger_add_output (logger, "asfilehasher.log");

	AS_DBG ("HASHER: Logging subsystem started");

	/* load old shares if any */
	if ((fp = fopen (shares_file, "r")))
	{
		AS_DBG_1 ("HASHER: Reading old shares from '%s'\n", shares_file);

		nfiles = 0;
		while (fgets (buf, sizeof (buf), fp))
		{
			char hash64[AS_HASH_BASE64_SIZE];
			char path[PATH_MAX];
			Share *share;

			if (strlen (buf) >= sizeof (buf) - 1)
			{
				AS_ERR ("HASHER: Aborting old share load. Line too long.");
				break;
			}

			if (!(share = malloc (sizeof(Share))))
			{
				AS_ERR ("HASHER: Fatal: No memory for share.");
				exit (1);
			}

			/* <hash> <size> <last_modify> <path> */
			if (sscanf (buf, "%29s %u %lu %[^\r\n]", hash64, &share->size,
				&share->last_modify, path) != 4)
			{
				AS_ERR ("HASHER: sscanf failed for old share");
				free (share);
				continue;
			}

			if (!(share->hash = as_hash_decode (hash64)))
			{
				AS_ERR ("HASHER: Failed to create hash for old share.");
				free (share);
				continue;
			}

			share->path = strdup (path);

			old_shares = list_prepend (old_shares, share);
			nfiles++;
		}

		fclose (fp);
		AS_DBG_1 ("HASHER: Loaded %d old shares.", nfiles);	
	}

	/* recursively collect new files */
	AS_DBG_1 ("HASHER: Collecting files in '%s'.", share_root);	
	nfiles = 0;
	nfiles = path_traverse (&new_shares, nfiles, share_root);
	AS_DBG_1 ("HASHER: Collected %d files.", nfiles);


	/* merge hashes from old shares if possible */
	AS_DBG ("HASHER: Merging hashes from old shares...");
	nfiles = 0;

	for (ln = new_shares; ln; ln = ln->next)
	{
		Share *new_share = ln->data;

		if (new_share->hash)
			continue;

		/* FIXME: O(n^2) */
		for (lo = old_shares; lo; lo = lo->next)
		{
			Share *old_share = lo->data;

			if (strcmp (old_share->path, new_share->path) == 0 &&
			    old_share->last_modify == new_share->last_modify &&
				old_share->size == new_share->size)
			{
				/* copy hash */
				new_share->hash = as_hash_copy (old_share->hash);
				assert (new_share->hash);
				nfiles++;
			}
		}	
	}
	AS_DBG_1 ("HASHER: Merged %d hashes.", nfiles);

	/* Hash remaining files without hash */
	AS_DBG ("HASHER: Hashing remaining files...");
	nfiles = 0;

	for (ln = new_shares; ln; ln = ln->next)
	{
		Share *new_share = ln->data;
		ASShare *as_share;

		if (new_share->hash)
			continue;

		AS_DBG_1 ("HASHER: Hashing '%s'", new_share->path);

		if (!(as_share = as_share_create (new_share->path, NULL, NULL,
		                                  new_share->size, REALM_UNKNOWN)))
		{
			AS_ERR_1 ("HASHER: Couldn't hash file '%s'", new_share->path);
			continue;			
		}

		new_share->hash = as_hash_copy (as_share->hash);
		as_share_free (as_share);
		nfiles++;
	}
	AS_DBG_1 ("HASHER: Hashed %d files.", nfiles);

	/* Save shares in file. */
	AS_DBG ("HASHER: Saving shares in share file...");
	nfiles = 0;

	if (!(fp = fopen (shares_file, "w")))
	{
		AS_ERR_1 ("HASHER: Failed to open shares file '%s' for writing\n", shares_file);
		exit (1);
	}

	for (ln = new_shares; ln; ln = ln->next)
	{
		Share *new_share = ln->data;
		char *hash_str;

		if (!new_share->hash)
			continue;

		hash_str = as_hash_encode (new_share->hash);
		assert (hash_str);

		/* <hash> <size> <last_modify> <path> */
		fprintf (fp, "%s %u %lu %s\n", hash_str, new_share->size,
		         new_share->last_modify, new_share->path);

		free (hash_str);
		nfiles++;
	}
	fclose (fp);
	AS_DBG_1 ("HASHER: Saved %d shares.", nfiles);

	/* Save Ares links */
	if (server_ip == INADDR_NONE || server_port == 0)
	{
		AS_DBG_1 ("HASHER: Saving Ares links in '%s' with no source information",
	              link_file);
	}
	else
	{
		AS_DBG_3 ("HASHER: Saving Ares links in '%s' with source %s:%u",
	              link_file, net_ip_str (server_ip), server_port);
	}
	nfiles = 0;

	if (!(fp = fopen (link_file, "w")))
	{
		AS_ERR_1 ("HASHER: Failed to open link file '%s' for writing\n", link_file);
		exit (1);
	}

	for (ln = new_shares; ln; ln = ln->next)
	{
		Share *new_share = ln->data;
		char *link;

		if (!new_share->hash)
			continue;

		if(!(link = arlnk_from_share (new_share, server_ip, server_port)))
		{
			AS_ERR_1 ("HASHER: Couldn't create Ares link for '%s'", new_share->path);
			continue;
		}

		fprintf (fp, "warez://%s %s\n", link, new_share->path);

		free (link);
		nfiles++;
	}
	fclose (fp);
	AS_DBG_1 ("HASHER: Saved %d links.", nfiles);


	/* shutdown */
	as_logger_free (logger);

	/* FIXME: this leaks member memory */
	list_free (old_shares);
	list_free (new_shares);

	return 0;
}

/*****************************************************************************/

/* returns the total files read plus the old passed as arg 'total' */
static int path_traverse (List **new_shares, int total, char *path)
{
	DIR           *dir;
	struct dirent *d;
	struct stat    st;
	char           newpath[PATH_MAX];

	if (!path)
		return total;

	AS_DBG_1 ("HASHER: Descending %s...", path);

	/* calculate the length here for optimization purposes */
	if (!(dir = opendir (path)))
	{
		AS_WARN_1 ("HASHER: Cannot open dir %s.", path);
		return total;
	}

	while ((d = readdir (dir)))
	{
		if (!d->d_name || !d->d_name[0])
			continue;

		/* Hide dot files */
		if (d->d_name[0] == '.')
			continue;

		/* ignore '.' and '..' */
		if (!strcmp (d->d_name, ".") || !strcmp (d->d_name, ".."))
			continue;

		snprintf (newpath, sizeof (newpath) - 1, "%s/%s", path, d->d_name);

		/* stat file */
		if (stat (newpath, &st) == -1)
		{
			AS_WARN_1 ("HASHER: Cannot stat %s", newpath);
			continue;
		}

		if (S_ISDIR (st.st_mode))
		{
			total = path_traverse (new_shares, total, newpath);
		}
		else if (S_ISREG (st.st_mode))
		{
			Share *share;

			if (!(share = malloc (sizeof (Share))))
			{
				AS_ERR ("HASHER: Fatal: No memory for new share.");
				exit (1);
			}

			share->path = strdup (newpath);
			share->size = st.st_size;
			share->last_modify = st.st_mtime;
			share->hash = NULL;

			*new_shares = list_prepend (*new_shares, share);
			total++;
		}
	}

	closedir (dir);

	return total;
}

/*****************************************************************************/

/* creates ares link from share with ip/port as source */
static char *arlnk_from_share (Share *share, in_addr_t ip, in_port_t port)
{
	ASPacket *packet;
	char *link = NULL;
	int i;

	if (!(packet = as_packet_create ()))
		return NULL;

	/* leading zeros */
	for (i = 0; i < 16; i++)
		as_packet_put_8 (packet, 0x00);

	/* file size */
	as_packet_put_le32 (packet, share->size);

	/* file name */
	as_packet_put_strnul (packet, as_get_filename (share->path));

	/* add source info if valid */
	if (ip != INADDR_NONE && ip != 0 && port != 0)
	{
		as_packet_put_8 (packet, 0x07);
		as_packet_put_le32 (packet, ip);
		as_packet_put_le16 (packet, port);
	}

	/* hash */
	as_packet_put_8 (packet, 0x08);
	as_packet_put_hash (packet, share->hash);

#if 0
	/* dump packet in plain */
	as_packet_dump (packet);
#endif

	/* compress packet */
	if (!as_packet_compress (packet))
	{
		as_packet_free (packet);
		return NULL;
	}

	/* encrypt packet */
	as_encrypt_arlnk (packet->data, packet->used);

	/* base 64 encode link */
	if (!(link = as_base64_encode (packet->data, packet->used)))
	{
		as_packet_free (packet);
		return NULL;
	}

	as_packet_free (packet);
	return link; 
}

/*****************************************************************************/
