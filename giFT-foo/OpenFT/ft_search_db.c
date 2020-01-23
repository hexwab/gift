/*
 * $Id: ft_search_db.c,v 1.63 2003/06/26 12:08:48 jasta Exp $
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

#include <stddef.h>                    /* offsetof */

#include "ft_openft.h"

#include "src/meta.h"

#include "ft_search.h"
#include "ft_search_exec.h"

#include "md5.h"

#include "ft_search_db.h"

/*****************************************************************************/

#ifdef USE_LIBDB

/*
 * God damn, why didn't they define this!?
 */
#define DB_VERSION \
    ((DB_VERSION_MAJOR << 16) | \
     (DB_VERSION_MINOR << 8) | \
     (DB_VERSION_PATCH))

/*
 * Sync all databases after changes have been made to ease debugging.  This
 * greatly hurts performance and should not be enabled for every day usage.
 */
/* #define SEARCH_DB_SYNC */

/*
 * Apply a custom hash function for the child data so that a well distributed
 * unique md5 hash will not be rehashed by libdb's generic routines.  Instead,
 * use a routine that will directly copy the first 4 bytes of the md5.
 *
 * NOTE:
 *
 * It has been determined that this causes a great deal of problems in DB 4.1
 * and presumably 4.0.  Once you create a database with a custom hash
 * function, it seems you no longer have the ability to remove it, even if
 * you reset the hash function prior to calling DB->remove.  It appears as
 * though DB 3.x is unaffected, but I have no further information than that.
 */
/* #define SEARCH_DB_DIRHASH */

/*****************************************************************************/

/*
 * All of the globally opened environment and database handlers go here.
 * Keep in mind that the actual share record data goes in a separate database
 * per user sharing the files, but all databases are opened within the same
 * file.
 */
static DB_ENV *search_env = NULL;
static DB     *tokens_idx = NULL;
static DB     *md5_idx    = NULL;

/*
 * Structure of the child data entries:
 *
 * [ Unique 16 BYTE MD5 ] => [ DataRec ]
 */
typedef struct
{
	off_t          size;               /* file size */
	u_int16_t      tokens;             /* offset of query tokens */
	u_int16_t      path;               /* offset of path */
	u_int16_t      mime;               /* offset of mime type */
	u_int16_t      meta;               /* offset of meta data */
	u_int16_t      data_len;           /* total data length */

	/*
	 * Complete linear record data.  The libdb code does NOT write all
	 * sizeof(data) bytes so be very careful with this field as its size is
	 * merely to represent the maximum rec size.
	 */
	unsigned char  data[8192];
} DataRec;

/*
 * Master Secondary Database:
 * [ 4 BYTE TOKEN ] => [ TokensRec ]
 */
typedef struct
{
	FTSearchDB   *sdb;                 /* yes, we are writing a raw ptr
	                                    * into the database */
	unsigned char md5[16];
} TokensRec;

/*****************************************************************************/

#define ERR_DB_NODE(method,ret,node) \
	FT->DBGFN (FT, "%s: %s failed: %s", \
	           ft_node_fmt(node), method, db_strerror(ret))

#define ERR_DB_SDB(method,ret,sdb) \
	ERR_DB_NODE(method,ret,sdb->node)

#define ERR_DB_PATH(method,ret,path,db) \
	FT->DBGFN (FT, "%s(%s:%s) failed: %s", method, path, db, db_strerror(ret))

#define ERR_DB(method,ret) \
	FT->DBGFN (FT, "%s failed: %s", method, db_strerror(ret))

/*****************************************************************************/

static Share *db_lookup_md5 (FTSearchDB *sdb, unsigned char *md5);
static BOOL db_close (FTSearchDB *sdb, int rm);
static BOOL db_sync (FTSearchDB *sdb);

/*****************************************************************************/

static FTSearchDB *search_db_new (FTNode *node)
{
	FTSearchDB *sdb;

	if (!(sdb = MALLOC (sizeof (FTSearchDB))))
		return NULL;

	node->session->search_db = sdb;
	sdb->node = node;

	return sdb;
}

static void search_db_free (FTSearchDB *sdb)
{
	assert (sdb != NULL);
	assert (sdb->db == NULL);
	assert (sdb->remove_curs == NULL);
	assert (sdb->remove_txn == NULL);

	/* this is also set to NULL by ft_session.c:ft_session_stop */
	if (sdb->node && sdb->node->session)
		sdb->node->session->search_db = NULL;

	free (sdb->dbname);
	free (sdb);
}

/*****************************************************************************/

static int open_db (DB *dbp, char *path, char *database,
                    DBTYPE type, u_int32_t flags, int mode)
{
	int ret;

	/* fucking libdb */
#if DB_VERSION >= 0x040100
	ret = dbp->open (dbp, NULL, path, database, type, flags, mode);
#else
	ret = dbp->open (dbp, path, database, type, flags, mode);
#endif

	FT->DBGFN (FT, "opened(%i) %p %s:%s", ret, dbp, path,
	           STRING_NOTNULL(database));

	if (ret != 0)
	{
		/* eep!  search node ran out of disk space! */
		assert (ret != DB_RUNRECOVERY);

		/* less severe error, hopefully */
		ERR_DB_PATH("DB->open", ret, path, database);
	}

	return ret;
}

static int remove_db (DB_ENV *dbenv, char *path, char *database)
{
#if DB_VERSION < 0x040100 || 1
	DB *dbp;
#endif
	int ret;

	FT->DBGFN (FT, "attempting to remove %s:%s", path, database);

#if DB_VERSION >= 0x040100 && 0
	if ((ret = search_env->dbremove (search_env, NULL, path, database, 0)))
	{
		FT->DBGFN (FT, "DB_ENV->dbremove (%s:%s) failed: %s",
		           path, database, db_strerror (ret));
		return ret;
	}
#else
	if ((ret = db_create (&dbp, dbenv, 0)) || !dbp)
		return ret;

	if ((ret = dbp->remove (dbp, path, database, 0)))
	{
		ERR_DB_PATH("DB->remove", ret, path, database);
		return ret;
	}
#endif

	return ret;
}

static int close_db (DB *dbp, char *path, char *database, int rm)
{
	int       ret = 0;
	u_int32_t flags = 0;

	/* pretend it was a success */
	if (!dbp)
		return 0;

	/* if we are going to be removing this database we will not want to sync
	 * any of the data as we dont care if it is corrupted */
	if (rm)
		flags |= DB_NOSYNC;

	FT->DBGFN (FT, "closing %p %s:%s(%i,%i)", dbp, path, database,
	           rm, (int)flags);

	if ((ret = dbp->close (dbp, flags)))
	{
		ERR_DB_PATH("DB->close", ret, path, database);
		return ret;
	}

	if (rm)
		remove_db (search_env, path, database);

	assert (ret == 0);
	return ret;
}

static int allow_dups (DB *dbp,
                       int (*cmpfunc) (DB *, const DBT *, const DBT *))
{
	int ret;

	if ((ret = dbp->set_flags (dbp, DB_DUP)))
		ERR_DB("DB->set_flags", ret);

	if (cmpfunc)
	{
		if ((ret = dbp->set_dup_compare (dbp, cmpfunc)))
			ERR_DB("DB->set_dup_compare", ret);

		if ((ret = dbp->set_flags (dbp, DB_DUPSORT)))
			ERR_DB("DB->set_flags", ret);
	}

	return TRUE;
}

static int delete_key_data (DBC *dbcp, DBT *key, DBT *data)
{
	int ret;

	/* position the cursor at this key/data set */
	if ((ret = dbcp->c_get (dbcp, key, data, DB_GET_BOTH)))
	{
		ERR_DB("DBcursor->c_get", ret);
		return FALSE;
	}

	/* now delete the current position */
	if ((ret = dbcp->c_del (dbcp, 0)))
	{
		ERR_DB("DBcursor->c_del", ret);
		return FALSE;
	}

	return TRUE;
}

static int make_dbt_partial (DBT *dbt, u_int32_t doff, u_int32_t dlen)
{
	if (!dbt)
		return FALSE;

	memset (dbt, 0, sizeof (DBT));
	dbt->flags = DB_DBT_PARTIAL;
	dbt->doff = doff;
	dbt->dlen = dlen;

	return TRUE;
}

/*****************************************************************************/

static char *db_pri_local_path (FTSearchDB *sdb, char **dbname)
{
	/* first time opening this database we need to generate a pseudo-unique
	 * name for the database so that it won't collide with any temporary
	 * databases being used at removal right now */
	if (!sdb->dbname)
	{
		sdb->dbname = stringf_dup ("%s:%u",
		                           net_ip_str (sdb->node->ip),
		                           (unsigned int)(time(NULL)));
	}

	/* we are actually opening multiple databases within one file here so
	 * we need to specify a unique database name to assist libdb in
	 * figuring out wtf we are talking about */
	if (dbname)
		*dbname = sdb->dbname;

	/* the environment will provide the complete path for us, so we can
	 * specify relative paths here, in fact i think we have to if using an
	 * environment */
	return "children.data";
}

#ifdef SEARCH_DB_DIRHASH
static u_int32_t pri_local_hash (DB *dbp, const void *bytes, u_int32_t length)
{
	u_int32_t hash = 0;

	/* make sure libdb isn't playing tricks on us */
	if (length == 0)
		return hash;

	/* directly copy the leading byte of the MD5SUM */
	memcpy (&hash, bytes, MIN (length, sizeof (hash)));
	return hash;
}
#endif /* SEARCH_DB_DIRHASH */

static DB *open_db_pri_local (FTSearchDB *sdb)
{
	DB   *dbp = NULL;
	char *path;
	char *dbname;
	int   ret;

	if (!(path = db_pri_local_path (sdb, &dbname)))
		return NULL;

	if (db_create (&dbp, search_env, 0) || !dbp)
		return NULL;

#ifdef SEARCH_DB_DIRHASH
	/* this database is going to use MD5SUMs as it's key values, and thus
	 * should use a direct hashing routine instead of the built in one which
	 * will be duplicating the efforts of MD5 for no reason */
	if ((ret = dbp->set_h_hash (dbp, pri_local_hash)))
		ERR_DB_SDB("DB->set_h_hash", ret, sdb);
#endif /* SEARCH_DB_DIRHASH */

	if ((ret = open_db (dbp, path, dbname, DB_HASH, DB_CREATE, 0664)) != 0)
	{
		close_db (dbp, path, dbname, TRUE);
		return NULL;
	}

	return dbp;
}

static DB *db_pri_local (FTSearchDB *sdb)
{
	if (!sdb)
		return NULL;

	if (!sdb->db)
		sdb->db = open_db_pri_local (sdb);

	return sdb->db;
}

static DB *db_master (DB **dbpp,
                      int (*cmpfunc) (DB *, const DBT *, const DBT *),
                      char *path, DBTYPE type)
{
	DB *dbp = NULL;

	/* already opened */
	if (*dbpp)
		return *dbpp;

	if (db_create (&dbp, search_env, 0) || !dbp)
		return NULL;

	allow_dups (dbp, cmpfunc);

	if (open_db (dbp, path, NULL, type, DB_CREATE, 0644) != 0)
		close_db (dbp, path, NULL, TRUE);
	else
		*dbpp = dbp;

	return *dbpp;
}

static int compare_host (DB *dbp, const DBT *a, const DBT *b)
{
	assert (a->size == sizeof (in_addr_t));
	assert (b->size == a->size);

	return memcmp (a->data, b->data, a->size);
}

static DB *db_pri ()
{
	DB *dbp;

	dbp = db_master (&md5_idx, compare_host, "md5.index", DB_BTREE);

	return dbp;
}

static int compare_md5 (DB *dbp, const DBT *a, const DBT *b)
{
	TokensRec *a_rec = a->data;
	TokensRec *b_rec = b->data;
	int     ret;

	assert (a->size == sizeof (TokensRec));
	assert (b->size == a->size);

	/* first compare md5 */
	if ((ret = memcmp (a_rec->md5, b_rec->md5, sizeof (a_rec->md5))))
		return ret;

	/* we still need the ability to insert two md5s from different hosts, so
	 * we have to compare the host here in order to allow the insert to
	 * succeed */
	return INTCMP (a_rec->sdb->node->ip, b_rec->sdb->node->ip);
}

static DB *db_sec ()
{
	DB *dbp;

	/* secondary database is insert sorted by MD5 */
	dbp = db_master (&tokens_idx, compare_md5, "tokens.index", DB_BTREE);

	return dbp;
}

/*****************************************************************************/

/* this function avoids using file_rmdir as some users may have setup a
 * symlink for this directory and we dont want to damage it */
static void clean_db_path (char *path)
{
	/* not a clue what this is, but it looks like slop that needs to be
	 * dealt with */
	file_unlink (stringf ("%s/__db.001", path));
	file_unlink (stringf ("%s/__db.002", path));

	/* unlink our databases */
	file_unlink (stringf ("%s/children.data", path));
	file_unlink (stringf ("%s/md5.index", path));
	file_unlink (stringf ("%s/tokens.index", path));
}

/*
 * Initialize the database environment wherein all databases for the search
 * node reside.  Really the only reason we are doing this is to share an
 * underlying memory cache so that we dont need to guess the distribution of
 * the cache across the three core databases.
 */
static BOOL db_init ()
{
	int       ret;
	u_int32_t flags = 0;
	u_int32_t gbytes;
	u_int32_t bytes;

	assert (search_env == NULL);

	/* we aren't checking the return value here because file_create_path
	 * sucks and needs to be rewritten :( */
	if (!file_mkdir (FT_SEARCH_ENV_PATH, 0755))
	{
		FT->err (FT, "unable to mkdir %s: %s",
		         FT_SEARCH_ENV_PATH, GIFT_STRERROR());
		return FALSE;
	}

	if ((ret = db_env_create (&search_env, 0)))
	{
		ERR_DB("db_env_create", ret);
		return FALSE;
	}

	/* set the cache size for the entire environment */
	gbytes = 0;
	bytes  = FT_SEARCH_ENV_CACHE;

	FT->DBGFN (FT, "search db params: path=%s, cache=%lu",
	           FT_SEARCH_ENV_PATH, (unsigned long)bytes);

	if ((ret = search_env->set_cachesize (search_env, gbytes, bytes, 0)))
	{
		ERR_DB("DB_ENV->set_cachesize", ret);
		return FALSE;
	}

	/* initialize the memory pool subsystem, make the database private, and
	 * specify that any necessary files should be created for us */
	flags = DB_INIT_MPOOL | DB_CREATE;

	if (FT_SEARCH_ENV_TXN)
		flags |= DB_INIT_TXN | DB_INIT_LOG;

	if (FT_SEARCH_ENV_PRIV)
		flags |= DB_PRIVATE;

	if ((ret = search_env->open (search_env, FT_SEARCH_ENV_PATH, flags, 0644)))
	{
		ERR_DB("DB_ENV->open", ret);
		return FALSE;
	}

	return TRUE;
}

static void db_destroy ()
{
	if (!search_env)
		return;

	search_env->close (search_env, 0);
	clean_db_path (FT_SEARCH_ENV_PATH);
}

/*****************************************************************************/

static u_int16_t serialize_fld (DataRec *rec, unsigned char *fld, size_t len)
{
	u_int16_t start;
	char      nul = 0;

	/* we must write at least one byte, make it a NUL */
	if (len == 0)
	{
		fld = &nul;
		len = sizeof (nul);
	}

	start = rec->data_len;

	if (rec->data_len + len >= sizeof (rec->data))
		return start;

	memcpy (rec->data + rec->data_len, fld, len);
	rec->data_len += len;

	return start;
}

static void pri_data_meta (ds_data_t *key, ds_data_t *value, DataRec *rec)
{
	assert (key->len > 0);
	assert (value->len > 0);

	serialize_fld (rec, key->data, key->len);
	serialize_fld (rec, value->data, value->len);
}

static u_int16_t serialize_tokens (DataRec *rec, uint32_t *tokens)
{
	u_int16_t ret;

	/* we really want to return the very first token write, not all of
	 * them */
	ret = rec->data_len;

	for (;; tokens++)
	{
		serialize_fld (rec, (unsigned char *)tokens, sizeof (*tokens));

		if (*tokens == 0)
			break;
	}

	return ret;
}

static int serialize_record (DBT *data, Hash *hash, uint32_t *tokens,
                             Share *share)
{
	static DataRec rec;

	rec.data_len = 0;

	/* copy the staticly sized fields */
	rec.size = share->size;
#if 0
	memcpy (rec.md5, sh->hash, sh->len);
#endif

	/* copy the variably sized fields */
	rec.tokens = serialize_tokens (&rec, tokens);
	rec.path = serialize_fld (&rec, share->path, STRLEN_0(share->path));
	rec.mime = serialize_fld (&rec, share->mime, STRLEN_0(share->mime));

	rec.meta = rec.data_len;
	share_foreach_meta (share, DS_FOREACH(pri_data_meta), &rec);

	/* apply some quazi-portable magic */
	data->data = &rec;
	data->size = sizeof (rec) - (sizeof (rec.data) - rec.data_len);

	return TRUE;
}

/*****************************************************************************/

static void import_meta (Share *share, char *meta, u_int16_t len)
{
	while (len != 0 && *meta)
	{
		char  *key;
		char  *val;
		size_t size = 0;

		key = meta + size; size += strlen (key) + 1;
		val = meta + size; size += strlen (val) + 1;
		meta += size;

		if (len < size)
			break;

		len -= size;

		share_set_meta (share, key, val);
	}
}

static Share *unserialize_record (FTSearchDB *sdb, unsigned char *md5,
                                  DataRec *rec)
{
	Share *share;

	if (!sdb || !rec)
		return NULL;

	share = ft_share_new (sdb->node, rec->size, md5, rec->data + rec->mime,
	                      rec->data + rec->path);

	if (!share)
		return NULL;

	import_meta (share, rec->data + rec->meta, rec->data_len - rec->meta);
	return share;
}

/*****************************************************************************/

static int db_insert_pri_local (FTSearchDB *sdb, Hash *hash,
                                uint32_t *tokens, Share *share)
{
	DB *dbp;
	DBT key;
	DBT data;
	int ret;

	/* access the appropriate database, opening if necessary */
	if (!(dbp = db_pri_local (sdb)))
	{
		FT->DBGFN (FT, "%s: unable to open primary local database",
		           ft_node_fmt (sdb->node));
		return FALSE;
	}

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = hash->data;
	key.size = hash->len;

	/* get the complete serialized record */
	if (!serialize_record (&data, hash, tokens, share))
		return FALSE;

	/* insert a unique entry, resulting in an error if data already exists at
	 * this key */
	if ((ret = dbp->put (dbp, NULL, &key, &data, DB_NOOVERWRITE)))
	{
		ERR_DB_SDB("DB->put", ret, sdb);
		return FALSE;
	}

	return TRUE;
}

static int db_insert_pri (FTSearchDB *sdb, Hash *hash, uint32_t *tokens,
                          Share *share)
{
	DB *dbp;
	DBT key;
	DBT data;
	int ret;

	/* just point a simple reference to that database in the master primary */
	if (!(dbp = db_pri ()))
		return FALSE;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data  = hash->data;
	key.size  = hash->len;
	data.data = &sdb;
	data.size = sizeof (sdb);

	if ((ret = dbp->put (dbp, NULL, &key, &data, DB_NODUPDATA)))
	{
		ERR_DB_SDB("DB->put", ret, sdb);
		return FALSE;
	}

	return TRUE;
}

static int sec_record (DBT *data, FTSearchDB *sdb, unsigned char *md5)
{
	static TokensRec rec;

	rec.sdb = sdb;
	memcpy (rec.md5, md5, sizeof (rec.md5));

	data->data = &rec;
	data->size = sizeof (rec);

	return TRUE;
}

static int db_insert_sec (FTSearchDB *sdb, Hash *hash, uint32_t *tokens,
                          Share *share)
{
	DB *dbp;
	DBT key;
	DBT data;
	int ret;

	if (!(dbp = db_sec ()))
		return FALSE;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	assert (hash->len == 16);
	if (!sec_record (&data, sdb, hash->data))
		return FALSE;

	/* insert one record for each token in the stream, effectively
	 * "pre-searching" for this file */
	for (; tokens && *tokens; tokens++)
	{
		DBC *dbcp;

		key.data = tokens;
		key.size = sizeof (*tokens);

		/* create a cursor because it seems we cant insert sorted without
		 * one...hmm, libdb is quirky */
		if (dbp->cursor (dbp, NULL, &dbcp, 0))
			continue;

		if ((ret = dbcp->c_put (dbcp, &key, &data, DB_KEYFIRST)))
		{
			if (ret != DB_KEYFIRST)
				ERR_DB_SDB("DBcursor->c_put", ret, sdb);
		}

		dbcp->c_close (dbcp);
	}

	return TRUE;
}

static BOOL db_insert (FTSearchDB *sdb, Share *share)
{
	Hash     *hash;
	uint32_t *tokens;

	/* make sure the master databases are open and ready to go */
	if (!db_pri() || !db_sec())
		return FALSE;

	if (!(hash = share_get_hash (share, "MD5")))
		return FALSE;

	if (!(tokens = ft_search_tokenizef (share)))
		return FALSE;

	/*
	 * Insert into the global primary and secondary databases, as well as the
	 * host-specific primary database.  See ft_search_db.h for more details
	 * on exactly how this stuff is designed, if you're willing to believe it
	 * was designed at all :)
	 */
	db_insert_pri_local (sdb, hash, tokens, share);
	db_insert_pri       (sdb, hash, tokens, share);
	db_insert_sec       (sdb, hash, tokens, share);

	free (tokens);

	return TRUE;
}

/*****************************************************************************/

static int db_remove_sec_token (DBC *dbcp, FTSearchDB *sdb, unsigned char *md5,
                                uint32_t token)
{
	DBT key;
	DBT data;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = &token;
	key.size = sizeof (token);

	if (!sec_record (&data, sdb, md5))
		return FALSE;

	return delete_key_data (dbcp, &key, &data);
}

static int db_remove_sec (FTSearchDB *sdb, uint32_t *tokens, unsigned char *md5)
{
	DB  *dbp;
	DBC *dbcp;
	int  ret = TRUE;

	if (!(dbp = db_sec()))
		return FALSE;

	/* construct a cursor so that we may use it for removing all tokens
	 * at once */
	if (dbp->cursor (dbp, NULL, &dbcp, 0))
		return FALSE;

	for (; tokens && *tokens; tokens++)
	{
		if (!(ret = db_remove_sec_token (dbcp, sdb, md5, *tokens)))
			break;
	}

	dbcp->c_close (dbcp);
	return ret;
}

static BOOL db_remove_pri_local (FTSearchDB *sdb, unsigned char *md5)
{
	DB *dbp;
	DBT key;
	DBT data;
	int ret;

	if (!(dbp = db_pri_local(sdb)))
		return FALSE;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = md5;
	key.size = 16;

	/* we don't have to be host-aware when removing from this database, as
	 * it is isolated away from the master */
	if ((ret = dbp->del (dbp, NULL, &key, 0)))
	{
		ERR_DB_SDB("DB->del", ret, sdb);
		return FALSE;
	}

	return TRUE;
}

static int db_remove_pri (FTSearchDB *sdb, unsigned char *md5)
{
	DB  *dbp;
	DBC *dbcp;
	DBT  key;
	DBT  data;
	int  ret;

	if (!(dbp = db_pri()))
		return FALSE;

	if (dbp->cursor (dbp, NULL, &dbcp, 0))
		return FALSE;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = md5;
	key.size = 16;

	data.data = &sdb;
	data.size = sizeof (sdb);

	ret = delete_key_data (dbcp, &key, &data);

	dbcp->c_close (dbcp);

	return ret;
}

static BOOL db_remove (FTSearchDB *sdb, unsigned char *md5, off_t *size)
{
	Share    *share;
	uint32_t *tokens;

	if (!db_pri() || !db_sec())
		return FALSE;

	db_remove_pri_local (sdb, md5);
	db_remove_pri (sdb, md5);

	/* grab the per-user data entry at the supplied key, which will contain
	 * enough information to get the token list */
	if (!(share = db_lookup_md5 (sdb, md5)))
		return FALSE;

	/* so that the caller knows how large the entry was that was just
	 * removed (for stats purposes) */
	if (size)
		*size = share->size;

	/* tokenize so that we know exactly what we're supposed to be removing
	 * from the secondary database */
	if ((tokens = ft_search_tokenizef (share)))
	{
		db_remove_sec (sdb, tokens, md5);
		free (tokens);
	}

	/* we only needed this for the token list...sigh */
	ft_share_unref (share);

	return TRUE;
}

/*****************************************************************************/

/* initialize the transaction and cursor data to ensure a clean removal of all
 * shares */
static BOOL init_removal (FTSearchDB *sdb)
{
	DB     *dbp;
	DBC    *dbcp;
	DB_TXN *txnid = NULL;
	int     ret;

	/* already initialized, dont do anything */
	if (sdb->db && sdb->remove_curs)
		return TRUE;

	if (!(dbp = db_pri_local (sdb)))
		return FALSE;

	assert (dbp == sdb->db);
	assert (sdb->remove_txn == NULL);
	assert (sdb->remove_curs == NULL);

	/* the user has to explicitly configure (at runtime) with transaction
	 * support to use it */
	if (FT_SEARCH_ENV_TXN)
	{
		/* construct the transaction id that will be used by the cursor for
		 * isolating removal */
#if DB_VERSION >= 0x040000
		if ((ret = search_env->txn_begin (search_env, NULL, &txnid, 0)))
#else
		if ((ret = txn_begin (search_env, NULL, &txnid, 0)))
#endif
		{
			ERR_DB_SDB("DBENV->txn_begin", ret, sdb);
			return FALSE;
		}

		sdb->remove_txn = txnid;
		assert (txnid != NULL);
	}

	/* create the cursor we will use to walk along the share host */
	if ((ret = dbp->cursor (dbp, txnid, &dbcp, 0)))
	{
		ERR_DB_SDB("DB->cursor", ret, sdb);
		return FALSE;
	}

	sdb->remove_curs = dbcp;
	assert (dbcp != NULL);

	return TRUE;
}

static BOOL destroy_removal (FTSearchDB *sdb)
{
	DBC    *dbcp;
	DB_TXN *txnid;
	int     ret = 0;

	dbcp  = sdb->remove_curs;
	txnid = sdb->remove_txn;

	if (dbcp)
	{
		if ((ret = dbcp->c_close (dbcp)))
			ERR_DB_SDB("DBcursor->c_close", ret, sdb);

		if (FT_SEARCH_ENV_TXN)
		{
			assert (txnid != NULL);

#if DB_VERSION >= 0x040000
			if ((ret = txnid->commit (txnid, 0)))
#else
			if ((ret = txn_commit (txnid, 0)))
#endif
				ERR_DB_SDB("DB_TXN->commit", ret, sdb);
		}
	}
	else if (txnid) /* && FT_SEARCH_ENV_TXN */
	{
#if DB_VERSION >= 0x040000
		if ((ret = txnid->abort (txnid)))
#else
		if ((ret = txn_abort (txnid)))
#endif
			ERR_DB_SDB("DB_TXN->abort", ret, sdb);
	}

	sdb->remove_curs = NULL;
	sdb->remove_txn = NULL;

	/* switch to bool retval */
	return (ret == 0);
}

/*
 * Remove a single share record from all databases, based on the cursor
 * associated with the search_db option.  Returning TRUE here means that the
 * removal has "completed", it does not indicate whether or not this was
 * because of an error.
 */
static BOOL db_remove_host (FTSearchDB *sdb)
{
	DBC      *dbcp;
	DBT       key;
	DBT       data;
	DBT       data_tokens;
	int       ret;
	u_int16_t tokens_len = 0;

	/* construct the cursor if necessary, if this fails we will simply abort
	 * the removal on error (which is a TRUE result from this func) */
	if (!init_removal (sdb))
		return TRUE;

	dbcp = sdb->remove_curs;
	assert (dbcp != NULL);

	memset (&key, 0, sizeof (key));

	/* initially we want to retrieve just the start position of the
	 * mime data so that we can figure out the length of the token data */
	make_dbt_partial (&data, offsetof (DataRec, path), sizeof (tokens_len));

	/* an uninitialized (unset) DB_NEXT c_get will result in a DB_FIRST call,
	 * so we don't really need to do any special call before we begin the
	 * loop */
	if ((ret = dbcp->c_get (dbcp, &key, &data, DB_NEXT)))
	{
		/* this is likely a complete (EOF) condition, but i really should
		 * be checking here...shame on me :) */
		return TRUE;
	}

	assert (data.size == sizeof (tokens_len));
	assert (key.size == 16);

	/* remove from the primary database immediately as we dont need to
	 * "complete" the lookup in order to get the md5sum */
	db_remove_pri (sdb, key.data);

	/* setup the data member to get the complete token data from the
	 * database */
	memcpy (&tokens_len, data.data, sizeof (tokens_len));
	make_dbt_partial (&data_tokens, offsetof (DataRec, data), tokens_len);

	/* retrieve the token list as a partial get */
	if ((ret = dbcp->c_get (dbcp, &key, &data_tokens, DB_CURRENT)))
	{
		ERR_DB_SDB("DBcursor->c_get", ret, sdb);
		return FALSE;
	}

	/* remove the all tokens in the list from the master secondary db */
	if (!db_remove_sec (sdb, (uint32_t *)(data_tokens.data), key.data))
	{
		FT->DBGFN (FT, "%s: cursor stability can no longer be guaranteed",
		           ft_node_fmt (sdb->node));
	}

	/* delete the current position to minimize the possibility of a race
	 * condition on this childs data (NOTE: this doesnt affect the cursor
	 * position, so the DB_NEXT is still required at the next call) */
	if ((ret = dbcp->c_del (dbcp, 0)))
		ERR_DB_SDB("DBcursor->c_del", ret, sdb);

	return FALSE;
}

static BOOL db_remove_host_timer (FTSearchDB *sdb)
{
	/*
	 * db_remove_host's return condition indicates whether or not the
	 * database entry is completely removed or a fatal error occurred.  Come
	 * to think of it, maybe we should differentiate the two :)
	 */
	if (!db_remove_host (sdb))
		return TRUE;

	/* we're finished removing, yay! */
	FT->DBGFN (FT, "%s: removed %lu shares",
	           ft_node_fmt (sdb->node), sdb->shares);

	/* destroy all the berkeley db specific stuff */
	destroy_removal (sdb);

	/* clean up the actual db */
	db_close (sdb, TRUE);
	db_sync (NULL);

	/* clean up the memory we allocated and attached to the node structure */
	search_db_free (sdb);

	/* nuke the timer */
	return FALSE;
}

/*****************************************************************************/

static BOOL db_sync (FTSearchDB *sdb)
{
#ifdef SEARCH_DB_SYNC
	DB *pri_local;
	DB *pri;
	DB *sec;

	if (sdb)
	{
		if ((pri_local = db_pri_local(sdb)))
			pri_local->sync (pri_local, 0);
	}

	if ((pri = db_pri()))
		pri->sync (pri, 0);

	if ((sec = db_sec()))
		sec->sync (sec, 0);
#endif /* SEARCH_DB_SYNC */

	return TRUE;
}

/*****************************************************************************/

static BOOL db_close (FTSearchDB *sdb, int rm)
{
	char *path;
	char *dbname;
	int   ret = 0;                     /* TODO: should be -1? */

	if (!sdb->db)
		return TRUE;

	assert (sdb->remove_curs == NULL);
	assert (sdb->remove_txn == NULL);

	if ((path = db_pri_local_path (sdb, &dbname)))
		ret = close_db (sdb->db, path, dbname, rm);

	sdb->db = NULL;

	return (ret == 0);
}

/*****************************************************************************/

static Share *db_lookup_md5 (FTSearchDB *sdb, unsigned char *md5)
{
	Share   *share;
	DataRec *rec;
	DB      *dbp;
	DBT      key;
	DBT      data;
	int      ret;

	if (!(dbp = db_pri_local(sdb)))
		return FALSE;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = md5;
	key.size = 16;

	if ((ret = dbp->get (dbp, NULL, &key, &data, 0)))
	{
		ERR_DB_SDB("DB->get", ret, sdb);
		return NULL;
	}

	assert (data.size <= sizeof (DataRec));
	rec = data.data;

	/*
	 * Create a new file structure for this record...this is actually quite a
	 * bit less efficient than we want to be, but ft_search_tokenizef needs
	 * to be called to gaurantee we are gathering the exact token stream that
	 * was created at insert.
	 */
	if (!(share = unserialize_record (sdb, md5, rec)))
		return NULL;

	return share;
}

/*****************************************************************************/

static DBC *cursor_pri_md5 (DB *dbp, unsigned char *md5)
{
	DBC *dbcp;
	DBT  key;
	DBT  data;
	int  ret;

	if ((ret = dbp->cursor (dbp, NULL, &dbcp, 0)))
	{
		ERR_DB("DB->cursor", ret);
		return NULL;
	}

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = md5;
	key.size = 16;

	/*
	 * Position the cursor at the beginning of the md5 set (containing share
	 * host ip addresses).  Unfortunately, we are actually reading the data
	 * element here and then simply throwing it away as I do not believe it
	 * is valid to request the cursor set without a data lookup.  Someone
	 * research this if you've got the time.
	 */
	if ((ret = dbcp->c_get (dbcp, &key, &data, DB_SET)))
	{
		dbcp->c_close (dbcp);
		return NULL;
	}

	return dbcp;
}

/*
 * This function is very poorly named.  What it actually means is lookup the
 * necessary information given an IP address and MD5 sum to construct a
 * completely unserialized FileShare structure and then add it to the search
 * results list.  This is utilized by both of the search interfaces.
 */
static int add_search_result (Array **a, FTSearchDB *sdb, unsigned char *md5)
{
	Share *share;

	/*
	 * Retrieve the actual share record containing enough data to
	 * completely unserialize the original FileShare object inserted into
	 * the database.
	 */
	if (!(share = db_lookup_md5 (sdb, md5)))
	{
		FT->DBGFN (FT, "%s: unable to lookup md5 %s",
		           ft_node_fmt (sdb->node), md5_fmt (md5));
		return FALSE;
	}

	/* last but not least, add the completely constructed file */
	if (!array_push (a, share))
		return FALSE;

	return TRUE;
}

static int db_search_md5 (Array **a, unsigned char *md5, int max_results)
{
	DB       *dbp;
	DBC      *dbcp;
	DBT       key;
	DBT       data;
	u_int32_t flags;
	int       results = 0;

	if (!(dbp = db_pri()))
		return 0;

	/*
	 * Create and position the cursor at the set resulting from the md5
	 * lookup in the master primary database.  In other words, a list of
	 * share host ip addresses that successfully matched this search.
	 */
	if (!(dbcp = cursor_pri_md5 (dbp, md5)))
		return 0;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	/* iterate the cursor */
	for (flags = DB_CURRENT;
	     dbcp->c_get (dbcp, &key, &data, flags) == 0;
	     flags = DB_NEXT_DUP)
	{
		FTSearchDB *sdb;

		/* make sure we're looking at an FTSearchDB ptr here */
		assert (data.size == sizeof (sdb));
		memcpy (&sdb, data.data, sizeof (sdb));

		/*
		 * Add the search result.  This should NOT fail, unless something
		 * very bad is going on, as we have already confirmed through this
		 * loop that the data we are looking for is inserted in at least the
		 * master database.  This database design does not utilize stale
		 * entries.
		 */
		if (add_search_result (a, sdb, md5))
		{
			/* if not provided, or provided as the initial value 0, do not
			 * obey any kind of maximum result set */
			if (max_results && --max_results <= 0)
				break;

			results++;
		}
	}

	/* tidy up */
	dbcp->c_close (dbcp);

	return results;
}

/*****************************************************************************/

struct cursor_stream
{
	DBC      *cursor;
	u_int32_t flags;
};

static int cleanup_matches (DBT *data, void *udata)
{
	free (data->data);
	free (data);
	return TRUE;
}

static int cleanup_cursors (struct cursor_stream *s, void *udata)
{
	(s->cursor)->c_close (s->cursor);
	free (s);
	return TRUE;
}

static void token_cleanup (List *matches, List *cursors)
{
	list_foreach_remove (matches, (ListForeachFunc)cleanup_matches, NULL);
	list_foreach_remove (cursors, (ListForeachFunc)cleanup_cursors, NULL);
}

static DBC *get_cursor (DB *dbp, uint32_t token)
{
	DBC *dbcp;
	DBT  key;
	DBT  value;
	int  ret;

	if (dbp->cursor (dbp, NULL, &dbcp, 0))
		return NULL;

	memset (&key, 0, sizeof (key));
	memset (&value, 0, sizeof (value));

	key.data = &token;
	key.size = sizeof (token);

	/* TODO: how do I set the cursor without actually requiring a lookup of
	 * this data? */
	if ((ret = dbcp->c_get (dbcp, &key, &value, DB_SET)) != 0)
	{
		dbcp->c_close (dbcp);
		return NULL;
	}

	return dbcp;
}

static List *token_gather_cursors (DB *dbp, uint32_t *tokens)
{
	uint32_t *t;
	List      *cursors = NULL;
	DBC       *dbcp;

	for (t = tokens; t && *t; t++)
	{
		struct cursor_stream *s;

		/* one of the tokens searched for failed to lookup completely,
		 * abort the search (and return 0 results) */
		if (!(dbcp = get_cursor (dbp, *t)))
		{
			token_cleanup (NULL, cursors);
			return NULL;
		}

		if (!(s = malloc (sizeof (struct cursor_stream))))
			continue;

		s->cursor = dbcp;
		s->flags  = DB_CURRENT;

		cursors = list_prepend (cursors, s);
	}

	return cursors;
}

static void token_add_result (List **results, DBT *data)
{
	DBT *copy;

	/* make a complete allocated copy and append to the list */
	if (!(copy = MALLOC (sizeof (DBT))))
		return;

	copy->size = data->size;

	if (!(copy->data = malloc (copy->size)))
	{
		free (copy);
		return;
	}

	memcpy (copy->data, data->data, copy->size);

	*results = list_prepend (*results, copy);
}

static int look_for (struct cursor_stream *s, DBT *cmp_data)
{
	TokensRec *cmp_rec;
	TokensRec *rec;
	DBT key;
	DBT data;
	int cmp;

	assert (cmp_data->size == sizeof (TokensRec));
	cmp_rec = cmp_data->data;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	/*
	 * Attempt to set this cursor to a similar position as the start cursor,
	 * while attempting to locate any possible token intersection according
	 * to the md5sum (compare cmp_data vs data).
	 */
	for (; (s->cursor)->c_get (s->cursor, &key, &data, s->flags) == 0;
	     s->flags = DB_NEXT_DUP)
	{
		assert (data.size == sizeof (TokensRec));
		rec = data.data;

		cmp = memcmp (rec->md5, cmp_rec->md5, sizeof (cmp_rec->md5));

		/*
		 * We've gone too far, therefore we know for sure this data does
		 * not exist in the current set.
		 */
		if (cmp > 0)
		{
			/*
			 * This data set is beyond the current set we are looking for,
			 * so we will want to start the next search here.
			 */
			s->flags = DB_CURRENT;
			return FALSE;
		}

		/* matched, note that we will not reset flags as this exact position
		 * will be passed by the parent cursor as well */
		if (cmp == 0)
			return TRUE;
	}

	/* this set has exhausted, no more data left...we should really set
	 * some special cursor flag so that we stop searching this stream. */
	return FALSE;
}

static void calc_shortest (struct cursor_stream *s, void **args)
{
	db_recno_t *count = args[0];
	db_recno_t  scnt;
	int         ret;

	/*
	 * Retrieve the count from the database.  Hopefully this operation should
	 * be inexpensive so that we can justify it's usage.
	 */
	if ((ret = ((s->cursor)->c_count (s->cursor, &scnt, 0))))
	{
		ERR_DB("DBcursor->c_count", ret);
		return;
	}

	/*
	 * This cursor's length is shorter than the last known cursor stream, so
	 * we should reset the smallest length and current "located" stream
	 * value on the args data.
	 */
	if (*count == 0 || scnt < *count)
	{
		*count = scnt;
		args[1] = s;
	}
}

static struct cursor_stream *get_start_cursor (List **qt)
{
	struct cursor_stream *s;
	List      *link;
	void      *args[2];
	db_recno_t count = 0;

	args[0] = &count;
	args[1] = NULL;

	/*
	 * Loop through all cursor streams in order to calculate the shortest
	 * cursor (in terms of number of duplicates).  See below (match_qt) for an
	 * explanation of why we do this.  Note that if we only have one
	 * element in this list we can assume it is the shortest and skip the
	 * cursor count retrieval.
	 */
	if (list_next (*qt))
		list_foreach (*qt, (ListForeachFunc)calc_shortest, args);

	/*
	 * If args[1] is non-NULL, we have located an appropriate node, remove from
	 * the cursor list and return the beginning cursor stream.  If args[1]
	 * is NULL, we should simply pop off the 0th element and return it.
	 */
	if (args[1])
		link = list_find (*qt, args[1]);
	else
		link = list_nth (*qt, 0);

	if (!link)
		return NULL;

	/* we need to assign this before we remove the link as it will be
	 * freed by removal */
	s = link->data;
	*qt = list_remove_link (*qt, link);

	return s;
}

static int match_qt (List **results, List **qt, int max_results)
{
	struct cursor_stream *s;
	List      *ptr;
	DBT        key;
	DBT        data;
	int        lost;
	size_t     matches = 0;

	if (!(*qt))
		return 0;

	/*
	 * The start cursor is the cursor used in the outer loop shown below.
	 * The number of duplicate entries in this cursor will determine the
	 * minimum number of loops required, and thus this function will attempt
	 * to locate the shortest (lowest count) cursor.
	 */
	if (!(s = get_start_cursor (qt)))
		return 0;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	/*
	 * Loop through the start cursor forcing all subsequent cursors to keep
	 * themselves aligned with this current position as determined by the
	 * sorting position of the md5sum it represents.  We are effectively
	 * trying to locate the intersection of all cursor sets so that we can
	 * guarantee a search hit.
	 */
	for (; (s->cursor)->c_get (s->cursor, &key, &data, s->flags) == 0;
	     s->flags = DB_NEXT_DUP)
	{
		lost = FALSE;

		/*
		 * Walk along all the other tokens looking for an intersection.  Note
		 * that this code holds the last position of the cursor so that we
		 * never have to begin the search from the beginning.
		 */
		for (ptr = *qt; ptr; ptr = list_next (ptr))
		{
			if (!look_for (ptr->data, &data))
			{
				lost = TRUE;
				break;
			}
		}

		/*
		 * This token matched all in all cursors.  It should be considered
		 * a positive hit.
		 */
		if (!lost)
		{
			token_add_result (results, &data);

			/* make sure we cap the size of the results
			 * TODO: this is a major bug here!  we cant cap the size of
			 * the results until after we apply the exclusion set! */
			matches++;

			if (max_results && matches >= max_results)
				break;
		}
	}

	/*
	 * Put the popped cursor back in the list so that we can still use
	 * the same interface for freeing all streams.
	 */
	*qt = list_prepend (*qt, s);

	return matches;
}

static int match_et (List **results, List **et, int max_results)
{
	if (!(*results) || !(*et))
		return 0;

	return 0;
}

static List *token_lookup_match (List *qt, List *et, int max_results)
{
	List *results = NULL;

	match_qt (&results, &qt, max_results);
	match_et (&results, &et, max_results);

	token_cleanup (NULL, qt);
	token_cleanup (NULL, et);

	return results;
}

static int lookup_ret (DBT *dbt, void **args)
{
	Array    **a           = args[0];
	int       *max_results = args[1];
	int       *matches     = args[2];
	TokensRec *rec;

	/* do not process more results than we were allowed */
	if (*max_results && *matches >= *max_results)
		return TRUE;

	assert (dbt->size == sizeof (TokensRec));
	rec = dbt->data;

	/*
	 * WARNING/TODO: We do not match realm here, and we REALLY NEED TO.
	 * Realm matches need to use a partial database lookup before the result
	 * is fully selected and added to the list.
	 */
	if (add_search_result (a, rec->sdb, rec->md5))
		(*matches)++;

	cleanup_matches (dbt, NULL);

	return TRUE;
}

static int token_lookup_ret (Array **a, List *cursors, char *realm,
                             int max_results)
{
	int nmatches = 0;
	void *args[] = { a, &max_results, &nmatches, realm };

	cursors = list_foreach_remove (cursors, (ListForeachFunc)lookup_ret, args);
	list_free (cursors);

	return nmatches;
}

/*
 * Perform a search through the query and exclude token sets.  This adds a
 * huge level of complexity to the search algorithm, and uses a specialized
 * master secondary database which effectively indexes all shares by each
 * individual lookup token.  Simple references to the per-user primary
 * database are kept in the result buckets here, so we can bypass the primary
 * master database entirely.
 *
 * If you are having trouble understanding this code, and I kind of expect
 * that, consult the meta documentation found in the header file.  Hopefully
 * a broad overview of exactly what we're trying to accomplish here will
 * help.
 */
static int db_search_tokens (Array **a, char *realm,
                             uint32_t *query, uint32_t *exclude,
                             int max_results)
{
	DB   *dbp;
    List *qt_cursors = NULL;
	List *et_cursors = NULL;
	List *cursors = NULL;
	int   results = 0;

	if (!(dbp = db_sec()))
		return 0;

	/* construct a list of all positioned cursors, effectively retrieving a
	 * list of token result streams */
	qt_cursors = token_gather_cursors (dbp, query);
	et_cursors = token_gather_cursors (dbp, exclude);

	/*
	 * Find the list of cursors which successfully matched this query by
	 * first identifying the intersection of all cursors within qt_cursors,
	 * and then excluding all matches from et_cursor.  Returns a newly
	 * allocated list containing all share host ip addresses and MD5s that
	 * matched the search.
	 *
	 * NOTE:
	 * The cursors list result is not in the same "format" as qt_cursors,
	 * the data held within is completely different.
	 */
	cursors = token_lookup_match (qt_cursors, et_cursors, max_results);

	/*
	 * Add all results to the main result list, after unserialization
	 * occurs.  This logic also handles cleanup of all non-returned data
	 * held within the cursors list.
	 */
	if (cursors)
		results = token_lookup_ret (a, cursors, realm, max_results);

	return results;
}

/*****************************************************************************/

#endif /* USE_LIBDB */

/*****************************************************************************/

BOOL ft_search_db_init ()
{
	BOOL ret = FALSE;

#ifdef USE_LIBDB
	/* delete any host shares left over from previous search node sessions */
	clean_db_path (FT_SEARCH_ENV_PATH);

	if (!(ret = db_init ()))
		ft_search_db_destroy ();
#endif /* USE_LIBDB */

	return ret;
}

void ft_search_db_destroy ()
{
#ifdef USE_LIBDB
	db_destroy ();
#endif /* USE_LIBDB */
}

/*****************************************************************************/

BOOL ft_search_db_insert (FTNode *node, Share *share)
{
	BOOL ret = FALSE;

	if (!node || !share)
		return FALSE;

#ifdef USE_LIBDB
	assert (node->session != NULL);
	assert (FT_SEARCH_DB(node) != NULL);

	if ((ret = db_insert (FT_SEARCH_DB(node), share)))
	{
		FT_SEARCH_DB(node)->shares++;
		FT_SEARCH_DB(node)->size += ((float)share->size / 1024.0) / 1024.0;
	}
#endif /* USE_LIBDB */

	return ret;
}

BOOL ft_search_db_remove (FTNode *node, unsigned char *md5)
{
	BOOL ret    = FALSE;
#ifdef USE_LIBDB
	off_t size = 0;
#endif /* USE_LIBDB */

	if (!node || !md5)
		return FALSE;

#ifdef USE_LIBDB
	if ((ret = db_remove (FT_SEARCH_DB(node), md5, &size)))
	{
		FT_SEARCH_DB(node)->shares--;
		FT_SEARCH_DB(node)->size -= ((float)size / 1024.0) / 1024.0;
	}
#endif /* USE_LIBDB */

	return ret;
}

BOOL ft_search_db_remove_host (FTNode *node)
{
	int ret = TRUE;

	if (!node)
		return FALSE;

#ifdef USE_LIBDB
	if (FT_SEARCH_DB(node))
	{
		assert (FT_SEARCH_DB(node)->db != NULL);

		FT->DBGFN (FT, "%s: scheduled removal", ft_node_fmt (node));

		/* initiate the timer to handle resource scheduling for us */
		timer_add (100 * MSEC, (TimerCallback)db_remove_host_timer,
		           FT_SEARCH_DB(node));
	}
#endif /* USE_LIBDB */

	return ret;
}

BOOL ft_search_db_sync (FTNode *node)
{
	BOOL ret = TRUE;

#ifdef USE_LIBDB
	ret = db_sync (FT_SEARCH_DB(node));
#endif /* USE_LIBDB */

	return ret;
}

/* doesn't actually open the database, rather it just creates the parent
 * structure to be used by all the internal operations */
BOOL ft_search_db_open (FTNode *node)
{
	if (!node)
		return FALSE;

#ifdef USE_LIBDB
	if (!FT_SEARCH_DB(node))
	{
		/* the internal interface will access the node object circularly to
		 * improve isolation of this interface */
		if (!(FT_SEARCH_DB(node) = search_db_new (node)))
			return FALSE;
	}
#endif /* USE_LIBDB */

	return TRUE;
}

/* this doesn't actually affect the master databases, simply the per-user
 * one associated w/ search db object */
BOOL ft_search_db_close (FTNode *node, int rm)
{
	BOOL ret = FALSE;

	if (!node)
		return FALSE;

#ifdef USE_LIBDB
	ret = db_close (FT_SEARCH_DB(node), rm);
#endif /* USE_LIBDB */

	return ret;
}

/*****************************************************************************/

Share *ft_search_db_lookup_md5 (FTNode *node, unsigned char *md5)
{
	Share *ret = NULL;

	if (!node || !md5)
		return NULL;

#ifdef USE_LIBDB
	ret = db_lookup_md5 (FT_SEARCH_DB(node), md5);
#endif /* USE_LIBDB */

	return ret;
}

int ft_search_db_md5 (Array **a, unsigned char *md5, int max_results)
{
	int results = 0;

	if (!md5 || max_results <= 0)
		return results;

#ifdef USE_LIBDB
	results = db_search_md5 (a, md5, max_results);
#endif /* USE_LIBDB */

	return results;
}

int ft_search_db_tokens (Array **a, char *realm,
                         uint32_t *query, uint32_t *exclude,
                         int max_results)
{
	int results = 0;

	if (!query)
		return results;

#ifdef USE_LIBDB
	results = db_search_tokens (a, realm, query, exclude, max_results);
#endif /* USE_LIBDB */

	return results;
}
