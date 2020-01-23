/*
 * sharing.c
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

#include <errno.h>

#include "gift.h"
#include "hook.h"

#include "hash.h"

#include "parse.h"
#include "conf.h"

#include "file.h"
#include "mime.h"
#include "md5.h"

#include "sharing.h"
#include "meta.h"
#include "id.h"

#include "upload.h"
#include "download.h"

#include "network.h"                   /* platform_child_proc */

/*****************************************************************************/

extern Config *gift_conf;

/*****************************************************************************/

/* build the share index in the background */
#define FORK_INDEX_BUILD

/*****************************************************************************/

struct _sroot
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

/* 0  == no uploads
 * -1 == unlimited */
static int max_uploads = -1;
static int disabled    = FALSE;

#define MAX_PERUSER_UPLOADS \
	config_get_int (gift_conf, "sharing/max_peruser_uploads=1")

/*****************************************************************************/

/* hide files with a leading . (UNIX hidden files) */
#define HIDE_DOT_FILES \
	config_get_int (gift_conf, "sharing/hide_dot_files=1")

/* share files as they are finished downloading */
#define SHARE_COMPLETED \
	config_get_int (gift_conf, "sharing/share_completed=1")

/*****************************************************************************/

/* number of bytes to read from the file to calculate the MD5 (0 means entire
 * file) */
#define MD5SUM_READ 0 /* 307200 */

/*****************************************************************************/
/* GENERIC API AVAILABLE TO PROTOCOLS */

int share_complete (FileShare *file)
{
	if (!file || !file->sdata)
		return FALSE;

	if (file->sdata->path && file->sdata->md5 && file->size)
	{
		if (file->sdata->path[0] != '/')
			return FALSE;
	}

	return TRUE;
}

static char *calculate_hpath (char *root, size_t root_len, char *filename)
{
	/* no root means we dont need an hpath */
	if (!root)
		return NULL;

	/* "" will trigger this, but its not exactly iteration so who cares */
	if (!root_len)
		root_len = strlen (root);

	return (filename + root_len);
}

static char *calculate_ext (char *path)
{
	char *ext;

	if (!path)
		return NULL;

	if ((ext = strrchr (path, '.')))
		ext++;

	return ext;
}

static ShareData *share_data_new (char *root, size_t root_len, char *path,
								  char *md5, time_t mtime)
{
	ShareData *sdata;

	if (!(sdata = malloc (sizeof (ShareData))))
		return NULL;

	/* i should probably have root handled as just a length into path, oh
	 * well */
	sdata->root  = STRDUP_N (root, root_len);
	sdata->path  = STRDUP (path);
	sdata->hpath = calculate_hpath (sdata->root, root_len, sdata->path);

	sdata->md5   = STRDUP (md5);

	return sdata;
}

FileShare *share_new (char *root, size_t root_len, char *path, char *md5,
                      unsigned long size, time_t mtime)
{
	FileShare *file;

	if (!(file = malloc (sizeof (FileShare))))
		return NULL;

	file->flushed = FALSE;

	file->sdata   = share_data_new (root, root_len, path, md5, mtime);

	/* TEMPORARY -- these will be moved up soon! */
	file->type    = mime_type     (file->sdata->path, NULL);
	file->mtime   = mtime;

	file->size    = size;
	file->local   = FALSE;
	file->data    = NULL;
	file->meta    = NULL;

	return file;
}

static void share_data_free (ShareData *sdata)
{
	if (!sdata)
		return;

	free (sdata->root);
	free (sdata->path);
	free (sdata->md5);

	free (sdata);
}

void share_free (FileShare *file)
{
	assert (file);

	if (file->local)
		dataset_clear (file->data);

	meta_free (file->meta);

	share_data_free (file->sdata);

	free (file);
}

/*****************************************************************************/
/* DATA INSERTION/LOOKUP CONVENTIONS FOR PROTOCOLS */

void *share_lookup_data (FileShare *file, char *protocol)
{
	if (!file || !protocol)
		return NULL;

	/* if this is locally shared, lookup the data in a Dataset */
	if (file->local)
		return dataset_lookup (file->data, protocol);

	/* otherwise, to conserve memory it's actually the data itself */
	return file->data;
}

void share_insert_data (FileShare *file, char *protocol, void *data)
{
	if (!file || !protocol)
		return;

	if (file->local)
	{
		dataset_insert (file->data, protocol, data);
		return;
	}

	/* see above for more info on why i do it this way */
	file->data = data;
}

void share_remove_data (FileShare *file, char *protocol)
{
	if (!file || !protocol)
		return;

	if (file->local)
	{
		dataset_remove (file->data, protocol);
		return;
	}

	file->data = NULL;
}

/*****************************************************************************/
/* SHARING ROOT MANIPULATION */

static int free_sroot (struct _sroot *sr, void *udata)
{
	free (sr->path);
	free (sr);

	return TRUE;
}

static void destroy_sroot (List *sroot)
{
	if (!sroot)
		return;

	/* TODO -- shouldnt we be freeing sroot->data as well? */
	list_foreach_remove (sroot, (ListForeachFunc) free_sroot, NULL);
}

static void add_sroot (List **sroot, char *root, int recurse)
{
	char          *ptr;
	struct _sroot *sr;

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

	if (!(sr = malloc (sizeof (struct _sroot))))
		return;

	sr->path    = STRDUP (root);
	sr->recurse = recurse;

	/* add the supplied path in host order.  see file_host_path for
	 * more details */
	*sroot = list_append (*sroot, sr);
}

static List *build_sroot ()
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
		add_sroot (&sroot, root, TRUE);

	free (sharing_root0);

	if (SHARE_COMPLETED)
	{
		char *unix_path;

		unix_path = file_unix_path (COMPLETED_PATH (("")));
		add_sroot (&sroot, unix_path, FALSE);
		free (unix_path);
	}

	return sroot;
}

static char *calculate_root (List *sroot, char *filename)
{
	struct _sroot *sr;
	List          *ptr;

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

/* give directions to the protocol when we change the local structure */
static void notify_protocol (FileShare *file, int command)
{
	List *ptr;

	for (ptr = protocol_list (); ptr; ptr = list_next (ptr))
	{
		Protocol *p = ptr->data;

		(*p->share) (file, command);
	}
}

/*****************************************************************************/
/* FILESHARE CREATION HELPERS */

static size_t sroot_len (char *root)
{
	char      *ptr;
	size_t     len;

	if (!root)
		return 0;

	len = ((ptr = strrchr (root, '/'))) ? ptr - root : 0;

	return len;
}

/* create a FileShare from data in the ~/.giFT/shares cache
 * NOTE: path is in UNIX file order */
static FileShare *create_share_cache (List *sroot, char *unix_path, char *md5,
                                      off_t size, time_t mtime)
{
	FileShare *file;
	char      *root;

	root = calculate_root (sroot, unix_path);
	file = share_new (root, sroot_len (root), unix_path, md5, size, mtime);

	return file;
}

/* create a FileShare from a file on disk
 * NOTE: root and path are in HOST file order */
static FileShare *create_share_disk (char *host_root, char *host_path)
{
	FileShare  *file;
	char       *unix_root;
	char       *unix_path;
	char       *md5;
	off_t       size;
	time_t      mtime;

	/* acquire the necessary information */
	if (!file_exists (host_path, &size, &mtime))
		return NULL;

	/* convert to UNIX file order
	 * NOTE: this is done first simply because md5_checksum is the most
	 * expensive operation here and should be last in case an error occurs
	 * elsewhere */
	unix_root = file_unix_path (host_root);
	if (!(unix_path = file_unix_path (host_path)))
		return NULL;

	if (!(md5 = md5_checksum (host_path, MD5SUM_READ)))
	{
		free (unix_root);
		free (unix_path);
		return NULL;
	}

	file = share_new (unix_root, sroot_len (unix_root), unix_path,
	                  md5, size, mtime);

	free (unix_root);
	free (unix_path);

	return file;
}

/*****************************************************************************/

static int cmp_list_entry (FileShare *file, char *path)
{
	if (!file->sdata)
		return -1;

	if (!strcmp (file->sdata->path, path))
		return 0;

	return -1;
}

static int cmp_hash_entry (char *md5, FileShare *file, char *path)
{
	return (cmp_list_entry (file, path) == 0);
}

static FileShare *find_share_path (Dataset **shares, List **duplicates,
                                   char *path)
{
	FileShare *file = NULL;

	if (!path)
		return NULL;

	if (shares)
		file = hash_table_find (*shares, (HashFunc) cmp_hash_entry, path);

	if (!file && duplicates)
	{
		List *link;
		link = list_find_custom (*duplicates, path, (CompareFunc) cmp_list_entry);
		file = (link) ? link->data : NULL;
	}

	return file;
}

static FileShare *find_share_hash (Dataset **shares, List **duplicates,
								   char *hash)
{
	FileShare *file = NULL;

	if (!hash)
		return NULL;

	if (shares)
		file = dataset_lookup (*shares, hash);

	if (!file && duplicates)
		/* TODO */;

	return file;
}

/*****************************************************************************/

static void add_dataset_share (Dataset **dataset, FileShare *file)
{
	if (!dataset)
		return;

	dataset_insert (*dataset, file->sdata->md5, file);
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

	if (!(cache = find_share_hash (shares, NULL, file->sdata->md5)))
		add_dataset_share (shares, file);
	else
		add_list_share (duplicates, file);

	return (cache == NULL);
}

/*****************************************************************************/

static void remove_dataset_share (Dataset **dataset, FileShare *file)
{
	if (!dataset)
		return;

	dataset_remove (*dataset, file->sdata->md5);
}

static int remove_share (Dataset **shares, List **duplicates, FileShare *file)
{
	remove_dataset_share (shares, file);
	share_free (file);
	return TRUE;
}

/*****************************************************************************/

static int handle_file (FILE *f, Dataset **shares, List **duplicates,
                        int p_notify, char *host_root, char *host_path,
                        off_t size, time_t mtime)
{
	FileShare *file  = NULL;
	FileShare *cache = NULL;
	FileShare *cache_s, *cache_d;
	char      *unix_path;
	int        usable;

	if (!(unix_path = file_unix_path (host_path)))
		return FALSE;

	if (!(cache_s = find_share_path (shares, NULL, unix_path)) &&
	    !(cache_d = find_share_path (NULL, duplicates, unix_path)))
	{
		file = create_share_disk (host_root, host_path);
	}
	else
	{
		cache = (cache_s) ? cache_s : cache_d;

		/* this path has been cached previously, if the mtimes differ remove
		 * it from the list and recalculate it as if it's a new file */
		if (cache->mtime != mtime)
		{
			remove_share (shares, duplicates, cache);
			file = create_share_disk (host_root, host_path);
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

	fprintf (f, "%lu %i %s %lu %s\n", mtime, usable,
	         file->sdata->md5, file->size, unix_path);

	free (unix_path);

	return TRUE;
}

static int path_traverse (FILE *f, Dataset **shares, List **duplicates,
                          int p_notify, char *root, char *path, int recurse)
{
	DIR           *dir;
	struct dirent *d;
	struct stat    st;
	size_t         path_len = 0;
	int            total    = 0;
	char           newpath[PATH_MAX];

	if (!path)
		return FALSE;

	/* calculate the length here for optimization purposes */
	path_len = strlen (path);

	dir = file_opendir (path);
	if (!dir)
	{
		GIFT_WARN (("cannot open dir %s", path));
		return FALSE;
	}

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

		snprintf (newpath, sizeof (newpath) - 1, "%s/%s", path, d->d_name);

		/*
		 * Might want to use lstat() instead of stat() or opendir(),
		 * because otherwise a symbolic link can cause a loop
		 * in directory structure.  Unfortunately, no symlinks in
		 * shares-dir then :-(
		 *
		 * NOTE <jasta> - there is a way to use lstat and stat, gnapster does
		 * it...but i cant remember how
		 */
		if (stat (newpath, &st) == -1)
		{
			TRACE (("cannot stat %s: %s", newpath, GIFT_STRERROR ()));
			continue;
		}

		if (S_ISDIR (st.st_mode) && recurse)
		{
			total += path_traverse (f, shares, duplicates, p_notify,
			                        root, newpath, recurse);
		}
		else if (S_ISREG (st.st_mode))
		{
			total += handle_file (f, shares, duplicates, p_notify,
			                      root, newpath, st.st_size, st.st_mtime);
		}
	}

	file_closedir (dir);

	return total;
}

static int share_build_index (Dataset **shares, List **duplicates, List *sroot,
                              int p_notify, unsigned long *r_files,
                              double *r_size)
{
	FILE    *f;
	char    *buf = NULL;
	unsigned long l_files = 0;
	double        l_size  = 0.0;

	TRACE_FUNC ();

	if (!(f = fopen (gift_conf_path ("shares"), "r")))
	{
		TRACE (("*** creating new shares file"));
		return TRUE;
	}

	while (file_read_line (f, &buf))
	{
		FileShare *file;
		char *pos = buf;
		int   usable;
		char *md5, *filename;
		unsigned long size;
		time_t mtime;

		trim_whitespace (pos);

		mtime    = ATOUL (string_sep (&pos, " "));
		usable   = ATOUL (string_sep (&pos, " "));
		md5      =        string_sep (&pos, " ");
		size     = ATOUL (string_sep (&pos, " "));
		filename = pos;

		if (str_isempty (filename) || str_isempty (md5))
			continue;

		if (!(file = create_share_cache (sroot, filename, md5, (off_t) size, mtime)))
			continue;

		if (add_share (shares, duplicates, file))
		{
			add_dataset_share (shares, file);

			/* FIXME!!!! */
			file->meta = id_file (filename);

			l_files++;
			l_size += ((float) file->size / 1024.0 / 1024.0);

			if (p_notify)
				notify_protocol (file, PROTOCOL_SHARE_ADD);
		}
	}

	fclose (f);

	if (r_files)
		*r_files = l_files;

	if (r_size)
		*r_size = l_size;

	return TRUE;
}

/*****************************************************************************/

static int clear_list_entry (FileShare *file, void *udata)
{
	notify_protocol (file, PROTOCOL_SHARE_REMOVE);
	share_free (file);
	return TRUE;
}

static int clear_dataset_entry (char *md5, FileShare *file, void *udata)
{
	return clear_list_entry (file, udata);
}

static void share_clear (Dataset **shares, List **duplicates)
{
	if (shares && *shares)
	{
		hash_table_foreach_remove (*shares, (HashFunc) clear_dataset_entry,
		                           NULL);
		hash_table_destroy (*shares);
		*shares = NULL;
	}

	if (duplicates && *duplicates)
	{
		list_foreach_remove (*duplicates, (ListForeachFunc) clear_list_entry,
		                     NULL);
		*duplicates = NULL;
	}
}

/*****************************************************************************/

static CHILD_FUNC (child_update_index)
{
	share_write_index ();

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

static void update_index (ChildFunc cfunc, ParentFunc pfunc, void *udata)
{
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
	SubprocessData *sdata;

	if (!(sdata = malloc (sizeof (SubprocessData))))
		return;

	sdata->fd = -1;
	sdata->cfunc = cfunc;
	sdata->pfunc = pfunc;
	sdata->udata = udata;

	(*sdata->cfunc) (sdata);
	(*sdata->pfunc) (NULL, 0, sdata->udata);

	free (sdata);
#endif /* FORK_INDEX_BUILD */
}

void share_update_index ()
{
	update_index ((ChildFunc) child_update_index,
	              (ParentFunc) parent_update_index, NULL);
}

/*****************************************************************************/

static CHILD_FUNC (child_add_index)
{
	FILE      *f;
	List      *sroot;
	char      *host_root, *unix_root;
	char      *host_path, *unix_path;

	if (!(f = fopen (gift_conf_path ("shares"), "a")))
		return FALSE;

	sroot = build_sroot ();

	/* eww, this is very messy */
	host_path = sdata->udata;
	unix_path = file_unix_path (host_path);

	unix_root = calculate_root (sroot, unix_path);
	host_root = file_host_path (unix_root);

	destroy_sroot (sroot);

	handle_file (f, NULL, NULL, FALSE, host_root, host_path, 0, 0);

	free (unix_path);
	free (host_root);

	fclose (f);

	return FALSE;
}

/* god this function is a hack */
void share_add_index (char *host_path)
{
	update_index ((ChildFunc) child_add_index,
	              (ParentFunc) parent_update_index, host_path);
}

static CHILD_FUNC (child_remove_index)
{
	/* TODO */
	return child_update_index (sdata);
}

void share_remove_index (char *host_path)
{
	update_index ((ChildFunc) child_remove_index,
	              (ParentFunc) parent_update_index, host_path);
}

/* output the ~/.giFT/shares file
 * NOTE: uses only local memory as it is expected to be run from a child
 * process */
void share_write_index ()
{
	FILE    *f;
	List    *ptr;
	List    *sroot;
	Dataset *shares     = NULL;
	List    *duplicates = NULL;
	char    *tmp_path;

	TRACE_FUNC ();

	sroot = build_sroot ();

	/* rebuild a data structure from the original file for use with the md5
	 * lookup in path_traverse */
	share_build_index (&shares, &duplicates, sroot, FALSE, NULL, NULL);

	tmp_path = STRDUP (gift_conf_path ("shares.tmp"));

	if (!(f = fopen (tmp_path, "w")))
	{
		free (tmp_path);
		return;
	}

	for (ptr = sroot; ptr; ptr = list_next (ptr))
	{
		struct _sroot *sr   = ptr->data;
		char          *root;

		/* we need to use the native host style path before we traverse here,
		 * otherwise, we're gonna be all fucked up with duplicated allocations
		 * from the conversion */
		if (!(root = file_host_path (sr->path)))
			continue;

		TRACE (("descending %s...", root));
		path_traverse (f, &shares, &duplicates, FALSE, root, root, sr->recurse);

		free (root);
	}

	fclose (f);

	file_mv (tmp_path, gift_conf_path ("shares"));
	free (tmp_path);

	destroy_sroot (sroot);

	share_clear (&shares, &duplicates);
}

/* read ~/.giFT/shares into a data structure
 * NOTE: affects program globals, expected to be executed from a parent
 * process */
Dataset *share_read_index ()
{
	List         *sroot;
	Dataset      *shares     = NULL;
	List         *duplicates = NULL;
	unsigned long files = 0;
	double        size  = 0.0;

	TRACE_FUNC ();

	/* get the protocol to flush it's shares w/ the server */
	notify_protocol (NULL, PROTOCOL_SHARE_FLUSH);

	/* clear any shares that existed before */
	share_clear (&local_shares, &local_duplicates);
	local_files = 0;
	local_size  = 0.0;

	sroot = build_sroot ();

	/* read from the file */
	share_build_index (&shares, &duplicates, sroot, TRUE, &files, &size);

	destroy_sroot (sroot);

	TRACE (("total shares: %lu (%.02fMB)", files, size));

	/* reset local variables */
	local_shares     = shares;
	local_duplicates = duplicates;
	local_files      = files;
	local_size       = size;

	/* sync back up */
	notify_protocol (NULL, PROTOCOL_SHARE_SYNC);

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
	return strcmp (a->sdata->path, b->sdata->path);
}

/* WARNING!  This is slow as hell! */
List *share_index_sorted ()
{
	List *index;

	index = list_sort (hash_flatten (local_shares),
	                   (CompareFunc) index_sort_cmp);

	return index;
}

/*****************************************************************************/

void share_foreach (HashFunc foreach_func, void *data)
{
	if (!local_shares)
		return;

	hash_table_foreach (local_shares, foreach_func, data);
}

/*****************************************************************************/

static int find_file (unsigned long key, FileShare *file, char *filename)
{
	if (!filename || !file->sdata || !file->sdata->hpath)
		return FALSE;

	return (strcmp (filename, file->sdata->hpath) == 0);
}

/* this function accepts a hidden path (/file.mp3) and returns the full
 * FileShare structure for it (the below function may be more appropriate for
 * you) */
FileShare *share_find_file (char *filename)
{
	FileShare *file;

	if (!filename)
		return NULL;

	file = hash_table_find (share_index (NULL, NULL),
							(HashFunc) find_file, filename);

	return file;
}

/* this function will accept file paths in the form:
 *
 * /usr/local/share/giFT/OpenFT/nodepage.html
 *
 * -OR -
 *
 * /file.txt (root path exclusion)
 *
 * return:
 *   on success, the fully qualified local pathname is set in full_path and
 *   returns 0.
 *   on failure, the error code is returned which is currently as follows:
 *
 *    -1  MAX_PERUSER_UPLOADS reached
 *    -2  max total uploads reached
 *    -3  this file is not authorized for upload, likely not being shared
 */
int share_auth_upload (char *user, char *filename, char **full_path)
{
	FileShare *file;
	char      *s_path;
	HookVar   *hv;
	int        hv_hint = -1;

	if (full_path)
		*full_path = NULL;

	/* reconstructs the path w/o '.' or '..' elements
	 * TODO -- I think Windows accepts '...', but ironically that's a valid
	 * filename on ext2 at least...rossta? */
	if (!(s_path = file_secure_path (filename)))
		return -3;

	if ((hv = hook_event ("upload_auth", TRUE,
	                      HOOK_VAR_STR, filename,
	                      HOOK_VAR_INT, upload_length (NULL),
	                      HOOK_VAR_INT, max_uploads,
	                      HOOK_VAR_NUL, NULL)))
	{
		if (hv->type == HOOK_VAR_INT)
		{
			TRACE (("hook event returned: %i", P_INT (hook_var_value (hv))));
		}

		hv_hint = P_INT (hook_var_value (hv));

		hook_var_free (hv);
	}

	/* this user has reached his limit ... force his client to queue the
	 * extra files */
	if (hv_hint == -1 && MAX_PERUSER_UPLOADS > 0 &&
	    upload_length (user) >= MAX_PERUSER_UPLOADS)
	{
		free (s_path);
		return -1;
	}

	/* before we authorize a legitimate file share, check total upload count */
	if ((hv_hint == -1 && max_uploads != -1 &&
	    upload_length (NULL) >= max_uploads) ||
	    hv_hint==-2)
	{
		free (s_path);
		return -2;
	}

	/* check to make sure we will actually share this file */
	file = share_find_file (s_path);
	free (s_path);

	if (!file || !file->sdata || !file->sdata->path)
		return -3;

	if (full_path)
		*full_path = STRDUP (file->sdata->path);

	return 0;
}

/*****************************************************************************/

void share_disable ()
{
	disabled = TRUE;
	max_uploads = share_status ();
}

void share_enable ()
{
	disabled = FALSE;
	max_uploads = share_status ();
}

int share_status ()
{
	if (disabled)
		return 0;

	/* sync with config */
	max_uploads = config_get_int (gift_conf, "sharing/max_uploads=-1");

	return max_uploads;
}
