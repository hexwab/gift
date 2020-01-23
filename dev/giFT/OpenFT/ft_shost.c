/*
 * ft_shost.c
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

#include "ft_openft.h"

#include "meta.h"
#include "stopwatch.h"

#include "ft_search.h"
#include "ft_search_exec.h"

#include "ft_shost.h"

/*****************************************************************************/

/*
 * Primary in-memory indexing all hosts sharing files to this node.
 * Optimizes by user for quick removal (fluctuating user base)
 *
 * NOTE:
 * This is the only aspect kept in memory, everything else is using libdb.
 */
static Dataset *shosts = NULL;

/*****************************************************************************/

#define MAX_CHILD_SHARES_HARD 64000    /* abort import on error */
#define MAX_CHILD_SHARES_SOFT 16000    /* restrict further inserts */

/*****************************************************************************/

Dataset *ft_shosts ()
{
	return shosts;
}

/*****************************************************************************/

FTSHost *ft_shost_new (int verified, in_addr_t host,
					   unsigned short ft_port, unsigned short http_port,
					   char *alias)
{
	FTSHost *shost;

	if (!(shost = MALLOC (sizeof (FTSHost))))
		return NULL;

	shost->verified     = verified;
	shost->host         = host;
	shost->ft_port      = ft_port;
	shost->http_port    = http_port;
	shost->alias        = STRDUP (alias);

	return shost;
}

#ifdef USE_LIBDB
static char *pri_db_path (FTSHost *shost)
{
	return gift_conf_path ("OpenFT/db/%s", net_ip_str (shost->host));
}

static char *sec_db_path (FTSHost *shost)
{
	return gift_conf_path ("OpenFT/db/%s.idx", net_ip_str (shost->host));
}

static void cleanup_db (DB *db, char *path)
{
	if (!db)
		return;

	db->close (db, 0);

	file_unlink (path);
}
#endif /* USE_LIBDB */

static int remove_active_files (FileShare *file, FTSHost *shost)
{
	FTShare *share;

	/* nullify the shost object -- this result is no longer valid but for
	 * design purposes we can't unload it from the search queue */
	if ((share = share_lookup_data (file, "OpenFT")))
		share->shost = NULL;

	return TRUE;
}

void ft_shost_free (FTSHost *shost)
{
#ifdef USE_LIBDB
	timer_remove (shost->pri_timer);

	cleanup_db (shost->pri, pri_db_path (shost));
	cleanup_db (shost->sec, sec_db_path (shost));
	cleanup_db (shost->ter, NULL);
#endif /* USE_LIBDB */

	list_foreach_remove (shost->files, (ListForeachFunc) remove_active_files,
	                     shost);

	free (shost->alias);
	free (shost);
}

/*****************************************************************************/

static FTSHost *lookup (in_addr_t ip)
{
	if (!ip)
		return NULL;

	return dataset_lookup (shosts, &ip, sizeof (ip));
}

static void insert (in_addr_t ip, FTSHost *shost)
{
	if (!ip || !shost)
		return;

	if (!shosts)
		shosts = dataset_new (DATASET_HASH);

	dataset_insert (&shosts, &ip, sizeof (ip), shost, 0);
}

static void del (in_addr_t ip)
{
	if (!ip)
		return;

	dataset_remove (shosts, &ip, sizeof (ip));
}

/*****************************************************************************/

FTSHost *ft_shost_get (in_addr_t ip)
{
	return lookup (ip);
}

void ft_shost_add (FTSHost *shost)
{
	if (!shost)
		return;

	if (lookup (shost->host))
		return;

	insert (shost->host, shost);
}

void ft_shost_remove (in_addr_t ip)
{
	FTSHost *shost;

	if (!(shost = lookup (ip)))
		return;

	del (ip);
	ft_shost_free (shost);
}

/*****************************************************************************/
/* SHARE DATABASE IMPORT */

#ifdef USE_LIBDB
static int compare_md5 (DB *dbp, const DBT *a, const DBT *b)
{
	return memcmp (a->data, b->data, a->size);
}

static DB *open_libdb (DBTYPE type, char *path, int allow_dup)
{
	DB       *dbp = NULL;
	int       ret;
	u_int32_t cache;

	if (db_create (&dbp, NULL, 0) || !dbp)
		return NULL;

	if (type == DB_BTREE)
	{
		if ((ret = dbp->set_bt_compare (dbp, compare_md5)))
			TRACE (("DB->set_bt_compare failed: %s", db_strerror (ret)));
	}

	if (allow_dup)
	{
		if ((ret = dbp->set_flags (dbp, DB_DUP)))
			TRACE (("DB->set_flags failed: %s", db_strerror (ret)));

		if ((ret = dbp->set_flags (dbp, DB_DUPSORT)))
			TRACE (("DB->set_flags failed: %s", db_strerror (ret)));
	}

	if (type == DB_BTREE)
		cache = 65536;                 /* 64K */
	else
		cache = 32768;                 /* 32K */

	if ((ret = dbp->set_cachesize (dbp, 0, cache, 0)))
	{
		TRACE (("DB->set_cachesize(%lu) failed: %s",
		        (unsigned long)cache, db_strerror (ret)));
	}

	/* fucking libdb */
#if DB_VERSION_MAJOR > 4 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 1)
	ret = dbp->open (dbp, NULL, path, NULL, type, DB_CREATE, 0664);
#else
	ret = dbp->open (dbp, path, NULL, type, DB_CREATE, 0664);
#endif

	assert (ret != DB_RUNRECOVERY);

	if (ret != 0)
	{
		TRACE (("DB->open failed: %s", db_strerror (ret)));
		dbp->close (dbp, 0);
		return NULL;
	}

	return dbp;
}

#if 0
static int close_pri (FTSHost *shost)
{
	assert (shost->pri_timer > 0);

	if (shost->pri)
		(shost->pri)->close (shost->pri, 0);

	shost->pri = NULL;
	shost->pri_timer = 0;

	return FALSE;
}
#endif

DB *ft_shost_pri (FTSHost *shost)
{
	if (!shost)
		return NULL;

	if (!shost->pri)
		shost->pri = open_libdb (DB_HASH, pri_db_path (shost), FALSE);

#if 0
	/*
	 * The primary database is not designed to remain open and actively
	 * pulling from the memory pool as it is used only after all matches
	 * have been confirmed.  After 20 minutes of no access (defined by
	 * usage of this function), the primary database will be closed and
	 * synced with disk.  Subsequent calls here will re-open the database
	 * as needed.
	 */
	if (shost->pri_timer)
		timer_reset (shost->pri_timer);
	else
	{
		shost->pri_timer =
			timer_add (20 * MINUTES, (TimerCallback)close_pri, shost);
	}
#endif

	return shost->pri;
}

DB *ft_shost_sec (FTSHost *shost)
{
	if (!shost)
		return NULL;

	if (!shost->sec)
		shost->sec = open_libdb (DB_BTREE, sec_db_path (shost), TRUE);

	return shost->sec;
}

int open_libdb_all (FTSHost *shost)
{
	if (shost->pri && shost->sec)
		return TRUE;

	TRACE (("%s", net_ip_str (shost->host)));

#if 0
	if (nrec > MAX_CHILD_SHARES_HARD)
	{
		TRACE (("hard maximum child shares reached, aborting import!"));
		return FALSE;
	}

	if (nrec > MAX_CHILD_SHARES_SOFT)
	{
		GIFT_WARN (("soft maximum child shares reached, importing %i",
		            (int)MAX_CHILD_SHARES_SOFT));
	}
#endif

	/* these assignments are very redundant, but it looks funky without them
	 * so i left them in */
	shost->pri = ft_shost_pri (shost);
	shost->sec = ft_shost_sec (shost);

	return (shost->pri && shost->sec);
}

void sync_libdb (DB *dbp)
{
	if (!dbp)
		return;

	dbp->sync (dbp, 0);
}

void sync_libdb_all (FTSHost *shost)
{
	sync_libdb (shost->pri);
	sync_libdb (shost->sec);
}

/*****************************************************************************/

static unsigned short pri_data_fld (FTShareRec *rec, char *fld, size_t fld_len)
{
	unsigned short start;
	char nul = 0;

	if (fld_len == 0)
	{
		fld     = &nul;
		fld_len = sizeof (nul);
	}

	start = rec->data_len;

	if (rec->data_len + fld_len >= sizeof (rec->data))
		return start;

	memcpy (rec->data + rec->data_len, fld, fld_len);
	rec->data_len += fld_len;

	return start;
}

static int pri_data_meta (Dataset *d, DatasetNode *node, FTShareRec *rec)
{
	pri_data_fld (rec, node->key, STRLEN_0 (node->key));
	pri_data_fld (rec, node->value, STRLEN_0 (node->value));
	return FALSE;
}

static void pri_data (DBT *data, ShareHash *sh, ft_uint32 *tokens,
                      FileShare *file)
{
	static FTShareRec rec;

	rec.data_len = 0;

	/* copy the staticly sized fields */
	rec.size = file->size;
	memcpy (rec.md5, sh->hash, sh->len);

	/* copy the variably sized fields */
	rec.path = pri_data_fld (&rec, SHARE_DATA(file)->path,
	                         STRLEN_0 (SHARE_DATA(file)->path));
	rec.mime = pri_data_fld (&rec, file->mime,
	                         STRLEN_0 (file->mime));

	rec.meta = rec.data_len;
	meta_foreach (file, DATASET_FOREACH (pri_data_meta), &rec);

	/* apply some magic */
	data->data = &rec;
	data->size = sizeof (FTShareRec) - (sizeof (rec.data) - rec.data_len);
}

static void import_pri (DB *db, ShareHash *sh, ft_uint32 *tokens,
                        FileShare *file)
{
	DBT key;
	DBT data;
	int ret;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = sh->hash;
	key.size = sh->len;

	pri_data (&data, sh, tokens, file);

	if ((ret = db->put (db, NULL, &key, &data, DB_NOOVERWRITE)))
	{
#if 0
		TRACE (("DB->put failed: %s", db_strerror (ret)));
#endif
		return;
	}
}

static int import_sec_cursor (DB *db, DBC *dbc, DBT *key, DBT *data)
{
	int ret;

	/* insert the data (sorted) at the supplied cursor location */
	if ((ret = dbc->c_put (dbc, key, data, DB_KEYFIRST)))
	{
		if (ret != DB_KEYEXIST)
			TRACE (("DBcursor->c_put failed: %s", db_strerror (ret)));

		return FALSE;
	}

	return TRUE;
}

static void import_sec (DB *db, ShareHash *sh, ft_uint32 *tokens,
                        FileShare *file)
{
	DBT key;
	DBT data;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	data.data = sh->hash;
	data.size = sh->len;

	for (; tokens && *tokens; tokens++)
	{
		DBC *dbcp;

		key.data = tokens;
		key.size = sizeof (*tokens);

		if (db->cursor (db, NULL, &dbcp, 0))
			continue;

		import_sec_cursor (db, dbcp, &key, &data);

		dbcp->c_close (dbcp);
	}
}

static void import_rec (FTSHost *shost, FileShare *file)
{
	ShareHash *sh;
	ft_uint32 *tokens;

	if (!(sh = share_hash_get (file, "MD5")))
		return;

	if ((tokens = ft_search_tokenizef (file)))
	{
		import_pri (shost->pri, sh, tokens, file);
		import_sec (shost->sec, sh, tokens, file);

		free (tokens);
	}
}
#endif /* USE_LIBDB */

static int shost_add_file (FTSHost *shost, FileShare *file)
{
	if (shost->shares >= MAX_CHILD_SHARES_SOFT)
		return FALSE;

#ifndef USE_LIBDB
	return FALSE;
#else /* USE_LIBDB */
	if (!open_libdb_all (shost))
		return FALSE;

	/* process this record */
	shost->shares++;
	shost->size += ((float)file->size / 1024.0) / 1024.0;

	import_rec (shost, file);
	return TRUE;
#endif /* !USE_LIBDB */
}

int ft_shost_add_file (FTSHost *shost, FileShare *file)
{
	if (!shost || !file)
		return FALSE;

	return shost_add_file (shost, file);
}

/*****************************************************************************/

#ifdef USE_LIBDB
static FTShareRec *lookup_pri_rec (FTSHost *shost, unsigned char *md5)
{
	DB *dbp;
	DBT key;
	DBT data;
	int ret;

	dbp = shost->pri;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = md5;
	key.size = 16;

	if ((ret = dbp->get (dbp, NULL, &key, &data, 0)))
	{
		TRACE (("DB->get: %s", db_strerror (ret)));
		return NULL;
	}

	return data.data;
}

static int remove_sec_rec (DBC *dbcp, unsigned char *md5, ft_uint32 token)
{
	DBT key;
	DBT data;
	int ret;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = &token;
	key.size = sizeof (token);

	data.data = md5;
	data.size = 16;

	if ((ret = dbcp->c_get (dbcp, &key, &data, DB_GET_BOTH)))
	{
		TRACE (("DBcursor->c_get: %s", db_strerror (ret)));
		return FALSE;
	}

	if ((ret = dbcp->c_del (dbcp, 0)))
	{
		TRACE (("DBcursor->c_del: %s", db_strerror (ret)));
		return FALSE;
	}

	return TRUE;
}

static int remove_pri_rec (FTSHost *shost, unsigned char *md5)
{
	DB *dbp;
	DBT key;
	DBT data;
	int ret;

	dbp = shost->pri;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = md5;
	key.size = 16;

	if ((ret = dbp->del (dbp, NULL, &key, 0)))
	{
		TRACE (("DB->del: %s", db_strerror (ret)));
		return FALSE;
	}

	return TRUE;
}

#endif /* USE_LIBDB */

static void import_meta (FileShare *file, char *meta, unsigned short len)
{
	while (len != 0 && *meta)
	{
		char  *key;
		char  *val;
		size_t size;

		key  = meta; meta += strlen (key) + 1;
		val  = meta; meta += strlen (val) + 1;
		size = (meta - key);

		if (len < size)
			break;

		len -= size;

		meta_set (file, key, val);
	}
}

FileShare *ft_record_file (FTShareRec *rec, FTSHost *shost)
{
	FileShare *file;

	file = ft_share_new (shost, rec->size, rec->md5, rec->data + rec->mime,
	                     rec->data + rec->path);

	if (!file)
		return NULL;

	import_meta (file, rec->data + rec->meta, rec->data_len - rec->meta);

	return file;
}

static int shost_remove_file (FTSHost *shost, unsigned char *md5)
{
#ifndef USE_LIBDB
	return FALSE;
#else /* USE_LIBDB */
	FileShare  *file;
	FTShareRec *rec;
	DBC        *dbcp;
	ft_uint32  *tokens, *ptr;

	if (!open_libdb_all (shost))
		return FALSE;

	if (!(rec = lookup_pri_rec (shost, md5)))
		return FALSE;

	if ((shost->sec)->cursor (shost->sec, NULL, &dbcp, 0))
		return FALSE;

	/*
	 * Create a new file structure for this record...this is actually quite
	 * a bit less efficient than we want to be, but ft_search_tokenizef
	 * needs to be called to gaurantee we are gathering the exact token
	 * stream that was created at insert.
	 */
	if (!(file = ft_record_file (rec, shost)))
		return FALSE;

	tokens = ft_search_tokenizef (file);
	ft_share_unref (file);

	/* remove all tokens from the secondary database */
	for (ptr = tokens; ptr && *ptr; ptr++)
		remove_sec_rec (dbcp, md5, *ptr);

	free (tokens);

	dbcp->c_close (dbcp);

	shost->shares--;
	shost->size -= ((float)rec->size / 1024.0) / 1024.0;

	return remove_pri_rec (shost, md5);
#endif /* !USE_LIBDB */
}

int ft_shost_remove_file (FTSHost *shost, unsigned char *md5)
{
	if (!shost || !md5)
		return FALSE;

	return shost_remove_file (shost, md5);
}

/*****************************************************************************/

int ft_shost_sync (FTSHost *shost)
{
	if (!shost)
		return FALSE;

#ifdef USE_LIBDB
	sync_libdb_all (shost);
#endif /* USE_LIBDB */

	return TRUE;
}

/*****************************************************************************/

void ft_shost_verified (in_addr_t ip, unsigned short ft_port,
                        unsigned short http_port, int verified)
{
	FTSHost *shost;

	if (!(shost = lookup (ip)))
		return;

	shost->ft_port   = ft_port;
	shost->http_port = http_port;
	shost->verified  = verified;
}

void ft_shost_avail (in_addr_t ip, unsigned long avail)
{
	FTSHost *shost;

	if (!(shost = lookup (ip)))
		return;

	shost->availability = avail;
}

int ft_shost_digest (in_addr_t ip, unsigned long *shares,
                     double *size, unsigned long *avail)
{
	FTSHost *shost;

	if (!(shost = lookup (ip)))
		return FALSE;

	if (shares)
		*shares = shost->shares;

	if (size)
		*size = shost->size;

	if (avail)
		*avail = shost->availability;

	return TRUE;
}
