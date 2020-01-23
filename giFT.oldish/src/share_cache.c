/*
 * share_cache.c
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

#include "gift.h"
#include "hook.h"

#include "parse.h"
#include "conf.h"
#include "network.h"

#include "file.h"
#include "mime.h"

#include "share_cache.h"
#include "share_db.h"

#include "meta.h"

#include "upload.h"
#include "download.h"

#include "if_share.h"

/*****************************************************************************/

extern Config *gift_conf;

/*****************************************************************************/

/* build the share index in the background */
#define FORK_INDEX_BUILD

#ifndef WIN32
#define TRACK_DIR_INODES
#endif /* !WIN32 */

/*****************************************************************************/

#ifdef TRACK_DIR_INODES
struct _dir_inode
{
	ino_t inode;
	dev_t dev;
};
#endif /* TRACK_DIR_INODES */

struct _share_root
{
	char         *path;
	unsigned char recurse : 1;
};

/*****************************************************************************/

static Dataset      *local_shares     = NULL;
static List         *local_duplicates = NULL;

/* cache the size of local_shares so that it doesn't need to be calculated
 * everytime OpenFT wants it */
static unsigned long local_files = 0;
static double        local_size  = 0.0; /* as MB */

/* set to TRUE anytime any operation is currently being executed on the
 * shares file
 * NOTE: this is used to block concurrent access */
static int           indexing = FALSE;

/*****************************************************************************/

/* hide files with a leading . (UNIX hidden files) */
#define HIDE_DOT_FILES \
	config_get_int (gift_conf, "sharing/hide_dot_files=1")

/* share files as they are finished downloading */
#define SHARE_COMPLETED \
	config_get_int (gift_conf, "sharing/share_completed=1")

/* follow symbolic links */
#define FOLLOW_SYMLINKS \
	config_get_int (gift_conf, "sharing/follow_symlinks=1")

/*****************************************************************************/

static int free_share_root (struct _share_root *sr, void *udata)
{
	free (sr->path);
	free (sr);

	return TRUE;
}

static void destroy_share_root (List *sroot)
{
	if (!sroot)
		return;

	/* TODO -- shouldnt we be freeing sroot->data as well? */
	list_foreach_remove (sroot, (ListForeachFunc) free_share_root, NULL);
}

static void add_share_root (List **sroot, char *root, int recurse)
{
	struct _share_root *sr;
	char *ptr;

	if (!root)
		return;

	/* make sure there are no trailing /'s */
	for (ptr = root + strlen (root) - 1;
	     ptr >= root && *ptr == '/'; ptr--);

	/* if no / was found, ptr[1] will equal 0 anyway */
	ptr[1] = 0;

	if (*root != '/')
	{
		GIFT_WARN (("invalid sharing root '%s': relative path", root));
		return;
	}

	if (!(sr = malloc (sizeof (struct _share_root))))
		return;

	sr->path    = STRDUP (root);
	sr->recurse = recurse;

	/* add the supplied path in host order.  see file_host_path for
	 * more details */
	*sroot = list_append (*sroot, sr);
}

static List *build_share_root ()
{
	List *sroot = NULL;
	char *sharing_root, *sharing_root0;
	char *root;

	if (!(sharing_root = config_get_str (gift_conf, "sharing/root")))
	{
		GIFT_WARN (("no sharing root found!!  share you bastard!"));
		return NULL;
	}

	sharing_root0 = sharing_root = STRDUP (sharing_root);

	while ((root = string_sep (&sharing_root, ":")))
		add_share_root (&sroot, file_expand_path (root), TRUE);

	free (sharing_root0);

	if (SHARE_COMPLETED)
	{
		char *unix_path;

		unix_path = file_unix_path (COMPLETED_PATH (("")));
		add_share_root (&sroot, unix_path, FALSE);
		free (unix_path);
	}

	return sroot;
}

static char *calculate_root (List *sroot, char *filename)
{
	struct _share_root *sr;
	List *ptr;

	if (!sroot || !filename)
		return NULL;

	for (ptr = sroot; ptr; ptr = list_next (ptr))
	{
		sr = ptr->data;

		/* found root */
		if (!strncmp (filename, sr->path, strlen (sr->path)))
			return sr->path;
	}

	return NULL;
}

/*****************************************************************************/
/* FILESHARE CREATION HELPERS */

static size_t share_root_len (char *root)
{
	char      *ptr;
	size_t     len;

	if (!root)
		return 0;

	len = ((ptr = strrchr (root, '/'))) ? ptr - root : 0;

	return len;
}

/* create a FileShare from a file on disk
 * NOTE: root and path are in HOST file order */
static FileShare *create_share_disk (char *host_root, char *host_path)
{
	FileShare     *file;
	char          *unix_root;
	char          *unix_path;
	off_t          size;
	time_t         mtime;

	/* acquire the necessary information */
	if (!file_exists (host_path, &size, &mtime) || size == 0)
		return NULL;

	/* convert to UNIX file order
	 * NOTE: this is done first simply because md5_checksum is the most
	 * expensive operation here and should be last in case an error occurs
	 * elsewhere */
	unix_root = file_unix_path (host_root);
	if (!(unix_path = file_unix_path (host_path)))
	{
		free (unix_root);
		return NULL;
	}

	file = share_new (NULL, unix_root, share_root_len (unix_root), unix_path,
	                  NULL, size, mtime);

	free (unix_root);
	free (unix_path);

	if (!file)
		return NULL;

	if (!hash_algo_run (file))
	{
		share_free (file);
		return NULL;
	}

	return file;
}

/*****************************************************************************/

static int cmp_list_entry (FileShare *file, char *path)
{
	if (!SHARE_DATA(file))
		return -1;

	if (!strcmp (SHARE_DATA(file)->path, path))
		return 0;

	return -1;
}

static int cmp_hash_entry (Dataset *d, DatasetNode *node, char *path)
{
	FileShare *file = node->value;

	return (cmp_list_entry (file, path) == 0);
}

static FileShare *find_share_path (Dataset **shares, List **duplicates,
                                   char *path)
{
	FileShare *file = NULL;

	if (!path)
		return NULL;

	if (shares)
		file = dataset_find (*shares, DATASET_FOREACH (cmp_hash_entry), path);

	if (!file && duplicates)
	{
		List *link;
		link = list_find_custom (*duplicates, path, (CompareFunc) cmp_list_entry);
		file = (link) ? link->data : NULL;
	}

	return file;
}

static FileShare *find_share_hash (Dataset **shares, List **duplicates,
                                   ShareHash *sh)
{
	FileShare *file = NULL;

	if (!sh)
		return NULL;

	if (shares)
		file = dataset_lookup (*shares, sh->hash, sh->len);

#if 0
	if (!file && duplicates)
		/* TODO */;
#endif

	return file;
}

/*****************************************************************************/

static void add_dataset_share (Dataset **dataset, FileShare *file)
{
	ShareHash *sh;

	if (!dataset)
		return;

	if (!(sh = share_hash_get (file, NULL)))
		return;

	if (!(*dataset))
		*dataset = dataset_new (DATASET_HASH);

	dataset_insert (dataset, sh->hash, sh->len, file, 0);
}

static void add_list_share (List **list, FileShare *file)
{
	if (!list)
		return;

	*list = list_prepend (*list, file);
}

/* adds the share to the appropriate structure
 * NOTE: return value is whether or not it was successfully added to
 * shares */
static int add_share (Dataset **shares, List **duplicates, FileShare *file)
{
	FileShare *cache;

	if (!share_complete (file))
		return FALSE;

	if (!(cache = find_share_hash (shares, NULL, share_hash_get (file, NULL))))
		add_dataset_share (shares, file);
	else
		add_list_share (duplicates, file);

	return (cache == NULL);
}

/*****************************************************************************/

static void remove_dataset_share (Dataset **dataset, FileShare *file)
{
	ShareHash *sh;

	if (!dataset)
		return;

	if (!(sh = share_hash_get (file, NULL)))
		return;

	dataset_remove (*dataset, sh->hash, sh->len);
}

static void remove_list_share (List **duplicates, FileShare *file)
{
	if (!duplicates)
		return;

	*duplicates = list_remove (*duplicates, file);
}

static int remove_share (Dataset **shares, List **duplicates, FileShare *file)
{
	remove_dataset_share (shares, file);
	remove_list_share (duplicates, file);
	share_unref (file);
	return TRUE;
}

/*****************************************************************************/

static int handle_file (SDB *db, Dataset **shares, List **duplicates,
                        char *host_root, char *host_path, off_t size,
                        time_t mtime)
{
	FileShare *file  = NULL;
	FileShare *cache = NULL;
	FileShare *cache_s = NULL;
	FileShare *cache_d = NULL;
	char      *unix_path;
	int        usable;

	if (!(unix_path = file_unix_path (host_path)))
		return FALSE;

	if (!(cache_s = find_share_path (shares, NULL, unix_path)) &&
	    !(cache_d = find_share_path (NULL, duplicates, unix_path)))
	{
		if (!(file = create_share_disk (host_root, host_path)))
		{
			free (unix_path);
			return FALSE;
		}
	}
	else
	{
		cache = (cache_s) ? cache_s : cache_d;

		/* this path has been cached previously, if the mtimes differ remove
		 * it from the list and recalculate it as if it's a new file */
		if (SHARE_DATA(cache)->mtime != mtime)
		{
			remove_share (shares, duplicates, cache);

			if (!(file = create_share_disk (host_root, host_path)))
			{
				free (unix_path);
				return FALSE;
			}
		}
	}

	if (!file)
	{
		file   = cache;
		usable = (cache_s) ? TRUE : FALSE;
	}
	else
	{
		/* if we did not successfully add to shares then this is a duplicate
		 * share (diff path, same hash) */
		usable = add_share (shares, duplicates, file);
	}

	/* make sure we have all algorithms accounted for
	 * NOTE: this is harmless if there is a value for all necessary
	 * types */
	hash_algo_run (file);

	/* no meta data, try again */
	if (!SHARE_DATA(file)->meta)
		meta_run (file);

	sdb_write_file (db, file);

	free (unix_path);
	return TRUE;
}

/*
 * Inform the interface protocol of a change in progress via the
 * socketpair provided from parent_child_proc.
 */
static void update_interface (int fd, int total)
{
	char *str;

	/* write the total as a string with the nul character for parsing ease on
	 * the parents side */
	str = stringf ("%i", total);
	net_send (fd, str, strlen (str) + 1);
}

static int path_traverse (SDB *db, int total, int fd,
                          Dataset **shares, List **duplicates,
                          Dataset **dir_inodes, char *root, char *path,
                          int recurse)
{
	DIR           *dir;
	struct dirent *d;
	struct stat    st;
	size_t         path_len = 0;
	char           newpath[PATH_MAX];
	char          *path_fmt;
	int            retval;

	if (!path)
		return FALSE;

	TRACE (("descending %s...", path));

	/* calculate the length here for optimization purposes */
	path_len = strlen (path);

	if (!(dir = file_opendir (path)))
	{
		GIFT_WARN (("cannot open dir %s: %s", path, GIFT_STRERROR ()));
		return FALSE;
	}

#ifndef WIN32
	path_fmt = "%s/%s";
#else
	path_fmt = "%s\\%s";
#endif

	while ((d = file_readdir (dir)))
	{
		if (!d->d_name || !d->d_name[0])
			continue;

		if (HIDE_DOT_FILES)
		{
			if (d->d_name[0] == '.')
				continue;
		}

		/* ignore '.' and '..' */
		if (!strcmp (d->d_name, ".") || !strcmp (d->d_name, ".."))
			continue;

		snprintf (newpath, sizeof (newpath) - 1, path_fmt, path, d->d_name);

#ifndef WIN32
		if (!FOLLOW_SYMLINKS)
			retval = lstat (newpath, &st);
		else
#endif /* !WIN32 */
			retval = stat (newpath, &st);

		if (retval == -1)
		{
			TRACE (("cannot stat %s: %s", newpath, GIFT_STRERROR ()));
			continue;
		}

		if (S_ISDIR (st.st_mode) && recurse)
		{
#ifdef TRACK_DIR_INODES
			struct _dir_inode di;

			memset (&di, 0, sizeof (di));
			di.inode = st.st_ino;
			di.dev = st.st_dev;

			/* refuse to descend this dir, inode matches */
			if (dataset_lookup (*dir_inodes, &di, sizeof (di)))
			{
				TRACE (("ignoring %s: already traversed this inode",
				        newpath));
				continue;
			}

			dataset_insert (dir_inodes, &di, sizeof (di), "dir_inode", 0);
#endif /* TRACK_DUP_INODES */

			total = path_traverse (db, total, fd, shares, duplicates, dir_inodes,
			                       root, newpath, recurse);
		}
		else if (S_ISREG (st.st_mode))
		{
			total += handle_file (db, shares, duplicates, root, newpath,
			                      st.st_size, st.st_mtime);
			update_interface (fd, total);
		}
	}

	file_closedir (dir);

	return total;
}

static int share_build_index (Dataset **shares, List **duplicates, List *sroot,
                              unsigned long *r_files, double *r_size)
{
	SDB          *db  = NULL;
	FileShare    *file;
	unsigned long l_files = 0;
	double        l_size  = 0.0;

	TRACE_FUNC ();

	if (!(db = sdb_new (gift_conf_path ("shares"), "rb")))
		return FALSE;

	if (!sdb_open (db))
		TRACE (("*** creating new shares file"));

	while ((file = sdb_read_file (db, NULL)))
	{
		if (add_share (shares, duplicates, file))
		{
			l_files++;
			l_size += ((float) file->size / 1024.0 / 1024.0);
		}
	}

	sdb_close (db);

	if (r_files)
		*r_files = l_files;

	if (r_size)
		*r_size = l_size;

	return TRUE;
}

/*****************************************************************************/

static int clear_list_entry (FileShare *file, void *udata)
{
	share_unref (file);
	return TRUE;
}

static int clear_dataset_entry (Dataset *d, DatasetNode *node, void *udata)
{
	FileShare *file = node->value;

	return clear_list_entry (file, udata);
}

static void share_clear (Dataset **shares, List **duplicates)
{
	if (shares && *shares)
	{
		dataset_foreach (*shares, DATASET_FOREACH (clear_dataset_entry), NULL);
		dataset_clear (*shares);
		*shares = NULL;
	}

	if (duplicates && *duplicates)
	{
		*duplicates =
		    list_foreach_remove (*duplicates,
		                         (ListForeachFunc) clear_list_entry,
		                         NULL);
	}
}

/*****************************************************************************/

static CHILD_FUNC (child_update_index)
{
	share_write_index (sdata->fd);

	/* parent gets called if we write to fd or close it by exiting */
	return TRUE;
}

static PARENT_FUNC (parent_update_index)
{
	share_read_index ();

	/* it's now safe to update again */
	indexing = FALSE;

	return FALSE;
}

/* update index and report progress to the interface protocol */
static PARENT_FUNC (parent_update_index_report)
{
	IFEvent  *event;
	IFEventID id = *(IFEventID *)udata;
	int       ret;

	event = if_event_lookup (id);

	/* do not abort on legit writes */
	if (len > 0)
	{
		/* sanity check the incoming data to make sure we wrote properly */
		assert (data[len - 1] == 0);

		if (event)
			if_share_action (event, "sync", data);

		return TRUE;
	}

	/* we are finished syncing, update everything */
	ret = parent_update_index (data, len, NULL);

	if (event)
	{
		if_share_action (event, "sync", "Done");
		if_share_finish (event);
	}

	free (udata);

	return ret;
}

static void update_index (ChildFunc cfunc, ParentFunc pfunc, void *udata)
{
#ifndef FORK_INDEX_BUILD
	SubprocessData *sdata;
#endif

	if (indexing)
	{
		GIFT_WARN (("ignoring update request because i feel like being pissy"));
		return;
	}

	indexing = TRUE;

#ifdef FORK_INDEX_BUILD
	if (!(platform_child_proc (cfunc, pfunc, udata)))
		TRACE (("UNABLE TO FORK!"));
#else /* !FORK_INDEX_BUILD */
	if (!(sdata = malloc (sizeof (SubprocessData))))
		return;

	sdata->fd = -1;
	sdata->cfunc = cfunc;
	sdata->pfunc = pfunc;
	sdata->udata = udata;

	indexing = TRUE;

	(*sdata->cfunc) (sdata);
	(*sdata->pfunc) (NULL, 0, sdata->udata);

	assert (indexing == FALSE);

	free (sdata);
#endif /* FORK_INDEX_BUILD */
}

void share_update_index ()
{
	IFEvent *event;

	GIFT_WARN (("updating index..."));

	if (!(event = if_share_new (NULL, 0)))
		return;

	if_share_action (event, "sync", "0");

	update_index ((ChildFunc) child_update_index,
	              (ParentFunc) parent_update_index_report,
	              memory_dup (&event->id, sizeof (event->id)));
}

/*****************************************************************************/

static CHILD_FUNC (child_add_entry)
{
	SDB       *db;
	List      *sroot;
	char      *host_root, *unix_root;
	char      *host_path, *unix_path;

	if (!(db = sdb_new (gift_conf_path ("shares"), "rb+")))
		return FALSE;

	if (!sdb_open (db))
	{
		if (!sdb_create (db))
		{
			sdb_close (db);
			return FALSE;
		}
	}

	sroot = build_share_root ();

	/* eww, this is very messy */
	host_path = sdata->udata;
	unix_path = file_unix_path (host_path);

	unix_root = calculate_root (sroot, unix_path);
	host_root = file_host_path (unix_root);

	destroy_share_root (sroot);

	handle_file (db, NULL, NULL, host_root, host_path, 0, 0);

	free (unix_path);
	free (host_root);

	sdb_close (db);

	return FALSE;
}

static PARENT_FUNC (parent_update_entry)
{
	int ret;

	ret = parent_update_index (data, len, udata);
	free (udata);

	return ret;
}

/* god this function is a hack */
void share_add_entry (char *host_path)
{
	update_index ((ChildFunc) child_add_entry,
	              (ParentFunc) parent_update_entry, STRDUP (host_path));
}

static CHILD_FUNC (child_remove_entry)
{
	/* TODO */
	return child_update_index (sdata);
}

void share_remove_entry (char *host_path)
{
	update_index ((ChildFunc) child_remove_entry,
	              (ParentFunc) parent_update_entry, STRDUP (host_path));
}

/* refreshes the share entry if it is stale.  returns FALSE if the entry
 * doesn't need to be updated
 * NOTE: this may be forked, you cannot rely on an immediate update! */
int share_update_entry (FileShare *file)
{
	char  *host_path;
	time_t mtime = 0;
	off_t  size  = 0;

	if (!file || !SHARE_DATA(file))
		return FALSE;

	host_path = file_host_path (SHARE_DATA(file)->path);

	/* this entry needs to be updated if the mtime on disk and in
	 * the structure do not match (or if the file no longer exists at all) */
	if (!file_exists (host_path, &size, &mtime) ||
		SHARE_DATA(file)->mtime != mtime || file->size != size)
	{
		free (host_path);
		share_update_index ();
		return TRUE;
	}

	free (host_path);
	return FALSE;
}

/*****************************************************************************/

/*
 * Output the ~/.giFT/shares file
 *
 * NOTE:
 * Uses only local memory as it is expected to be run from a child
 * process
 */
void share_write_index (int fd)
{
	SDB       *db;
	List      *ptr;
	List      *sroot;
	Dataset   *shares     = NULL;
	List      *duplicates = NULL;
	Dataset   *dir_inodes = NULL;
	char      *tmp_path;
	int        total = 0;

	TRACE_FUNC ();

	sroot = build_share_root ();

	/* rebuild a data structure from the original file for use with the md5
	 * lookup in path_traverse */
	share_build_index (&shares, &duplicates, sroot, NULL, NULL);

	if (!(db = sdb_new (gift_conf_path ("shares.tmp"), "wb+")))
	{
		GIFT_ERROR (("unable to open %s: %s", gift_conf_path ("shares.tmp"),
		             GIFT_STRERROR ()));
		return;
	}

	if (!sdb_create (db))
	{
		sdb_close (db);
		return;
	}

#ifdef TRACK_DIR_INODES
	dir_inodes = dataset_new (DATASET_HASH);
#endif

	/*
	 * Loop through all sharing root paths.  This will optionally include
	 * the download completed directory (non-recursively).
	 */
	for (ptr = sroot; ptr; ptr = list_next (ptr))
	{
		struct _share_root *sr = ptr->data;
		char *root;

		/*
		 * We need to use the native host style path before we traverse here,
		 * otherwise, we're gonna be all fucked up with duplicated allocations
		 * from the conversion.
		 */
		if (!(root = file_host_path (sr->path)))
			continue;

		TRACE (("descending root: %s...", root));
		total = path_traverse (db, total, fd, &shares, &duplicates,
		                       &dir_inodes, root, root, sr->recurse);

		free (root);
	}

#ifdef TRACK_DIR_INODES
	dataset_clear (dir_inodes);
#endif

	tmp_path = STRDUP (db->path);
	sdb_close (db);

	file_mv (tmp_path, gift_conf_path ("shares"));
	free (tmp_path);

	destroy_share_root (sroot);

	share_clear (&shares, &duplicates);
}

/*****************************************************************************/

static int emit_share_new (Dataset *d, DatasetNode *node, FileShare *file)
{
	Protocol *p = node->value;
	void     *data;

	/* make sure we can't have any data left over, this should probably
	 * be an assert */
	if ((data = share_lookup_data (file, p->name)))
	{
		p->share_free (p, file, data);
		share_remove_data (file, p->name);
	}

	/*
	 * Ask the protocol to create new arbitrary data (if it needs to be
	 * created) and then associate it with this structure.  If no data
	 * is provided we do not consider it an error.
	 */
	if ((data = p->share_new (p, file)))
		share_insert_data (file, p->name, data);

	return TRUE;
}

static int emit_share_new_all (Dataset *d, DatasetNode *node, void *udata)
{
	/* we need to emit share_new for all protocols, as well */
	protocol_foreach (DATASET_FOREACH (emit_share_new), node->value);
	return FALSE;
}

static int emit_share_free (Dataset *d, DatasetNode *node, FileShare *file)
{
	Protocol *p = node->value;
	void     *data;

	/*
	 * Delete any arbitrary data the protocol wanted associated here.  We
	 * must also remove it from this structure as we may not necessarily
	 * want to assume we are going to free the whole structure immediately
	 * after this.
	 */
	if ((data = share_lookup_data (file, p->name)))
	{
		p->share_free (p, file, data);
		share_remove_data (file, p->name);
	}

	return TRUE;
}

static int emit_share_free_all (Dataset *d, DatasetNode *node, void *udata)
{
	protocol_foreach (DATASET_FOREACH (emit_share_free), node->value);
	return FALSE;
}

static int emit_share_sync (Dataset *d, DatasetNode *node, int *begin)
{
	Protocol *p = node->value;

	/*
	 * Let the protocol know that we are entering the sync stage so that
	 * it may initialize whatever it needs to.  This same function is used
	 * (when *begin is FALSE) to terminate the sync.  Merely a convenience
	 * for protocols.
	 */
	p->share_sync (p, *begin);

	return TRUE;
}

/*****************************************************************************/

/*
 * Attempts to determine if data expressed by a_node exists in both dataset a
 * and b.  Returns TRUE if that is the case.
 *
 * This is done so that we can determine the practical differences between
 * the old set of files shared and the new.
 */
static int eq_filesets (Dataset *a, DatasetNode *a_node, Dataset *b)
{
	FileShare *a_file = a_node->value;
	FileShare *b_file;

	/*
	 * The file doesn't exist in the new set at all, therefore it must've
	 * been removed and we can safely return FALSE without any further
	 * checks.
	 */
	if (!(b_file = dataset_lookup (b, a_node->key, a_node->key_len)))
		return FALSE;

	/*
	 * If the mtime's of the files differ, giFT would've recalculated
	 * new data and these cannot be considered the same any longer.
	 */
	return (SHARE_DATA(a_file)->mtime == SHARE_DATA(b_file)->mtime);
}

static int emit_share_remove (Dataset *d, DatasetNode *node, FileShare *file)
{
	Protocol *p = node->value;

	/*
	 * Inform the protocol that we are no longer sharing the specified
	 * file.
	 */
	p->share_remove (p, file, share_lookup_data (file, p->name));

	return TRUE;
}

static int emit_share_remove_all (Dataset *former, DatasetNode *node,
                                  Dataset *latter)
{
	if (!eq_filesets (former, node, latter))
		protocol_foreach (DATASET_FOREACH (emit_share_remove), node->value);

	return FALSE;
}

static int emit_share_add (Dataset *d, DatasetNode *node, FileShare *file)
{
	Protocol *p = node->value;

	/*
	 * We have a new share for the protocol.  The protocol-specific data
	 * should have already been added, so this function isn't expected
	 * to do that again.
	 */
	p->share_add (p, file, share_lookup_data (file, p->name));

	return TRUE;
}

static int emit_share_add_all (Dataset *latter, DatasetNode *node,
                               Dataset *former)
{
	if (!eq_filesets (latter, node, former))
		protocol_foreach (DATASET_FOREACH (emit_share_add), node->value);

	return FALSE;
}

/*****************************************************************************/

static void handle_protocols (Dataset *former, Dataset *latter)
{
	int begin;

	/*
	 * Loop through all shares in the "new" set, emitting a share_new call
	 * to each protocol for each file in this set.
	 */
	dataset_foreach (latter, DATASET_FOREACH (emit_share_new_all), NULL);

	/*
	 * Now we must inform the protocol of all the modifications that occurred
	 * to our local repository which should be reflected on whichever
	 * network is implemented.
	 */
	begin = TRUE; protocol_foreach (DATASET_FOREACH (emit_share_sync), &begin);
	dataset_foreach (former, DATASET_FOREACH (emit_share_remove_all), latter);
	dataset_foreach (latter, DATASET_FOREACH (emit_share_add_all), former);
	begin = FALSE; protocol_foreach (DATASET_FOREACH (emit_share_sync), &begin);

	/*
	 * We must now inform the protocol of all the structures which will soon
	 * be deleted.
	 */
	dataset_foreach (former, DATASET_FOREACH (emit_share_free_all), NULL);
}

/*****************************************************************************/

/*
 * Read ~/.giFT/shares into a data structure
 *
 * NOTE:
 * Affects program globals, expected to be executed from a parent
 * process
 */
Dataset *share_read_index ()
{
	List         *sroot;
	Dataset      *shares     = NULL;
	List         *duplicates = NULL;
	unsigned long files = 0;
	double        size  = 0.0;

	TRACE_FUNC ();

	/* read from the file */
	sroot = build_share_root ();
	share_build_index (&shares, &duplicates, sroot, &files, &size);
	destroy_share_root (sroot);

	handle_protocols (local_shares, shares);

	/* clear any shares that existed before */
	share_clear (&local_shares, &local_duplicates);
	local_files = 0;
	local_size  = 0.0;

	TRACE (("total shares: %lu (%.02fMB)", files, size));

	/* reset local variables */
	local_shares     = shares;
	local_duplicates = duplicates;
	local_files      = files;
	local_size       = size;

	return shares;
}

/*****************************************************************************/

void share_clear_index ()
{
	share_clear (&local_shares, &local_duplicates);
}

/*****************************************************************************/

Dataset *share_index (unsigned long *files, double *size)
{
	if (files)
		*files = local_files;
	if (size)
		*size = local_size;

	return local_shares;
}

/*****************************************************************************/

static int index_sort_cmp (FileShare *a, FileShare *b)
{
	return strcmp (SHARE_DATA(a)->path, SHARE_DATA(b)->path);
}

/* WARNING!  This is slow as hell! */
List *share_index_sorted ()
{
	List *index;

	index = list_sort (dataset_flatten (local_shares),
	                   (CompareFunc) index_sort_cmp);

	return index;
}

/*****************************************************************************/

void share_foreach (DatasetForeach foreach_func, void *data)
{
	if (!local_shares)
		return;

	dataset_foreach_ex (local_shares, foreach_func, data);
}

/*****************************************************************************/

static int find_file (Dataset *d, DatasetNode *node, char *filename)
{
	FileShare *file = node->value;

	if (!filename || !SHARE_DATA(file) || !SHARE_DATA(file)->hpath)
		return FALSE;

	return (strcmp (filename, SHARE_DATA(file)->hpath) == 0);
}

/* this function accepts a hidden path (/file.mp3) and returns the full
 * FileShare structure for it (the below function may be more appropriate for
 * you) */
FileShare *share_find_file (char *filename)
{
	FileShare *file;

	if (!filename)
		return NULL;

	file = dataset_find (share_index (NULL, NULL),
						 DATASET_FOREACH (find_file), filename);

	return file;
}

/*****************************************************************************/

int share_indexing ()
{
	return indexing;
}
