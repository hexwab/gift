/*
 * $Id: ft_search_db.c,v 1.144 2005/03/24 21:54:28 hexwab Exp $
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

#if 0
#include <stddef.h>                    /* offsetof */
#endif

#include "ft_openft.h"

#include "ft_search.h"
#include "ft_tokenize.h"

#include "md5.h"

#include "ft_search_db.h"
#include "ft_routing.h"

/*****************************************************************************/

/* allow multiple calls to ft_search_db_init to be safe */
static BOOL db_initialized = FALSE;

/*****************************************************************************/

#ifdef USE_LIBDB

/*
 * God damn, why didn't they define this!?
 */
#define DB_VERSION \
    ((DB_VERSION_MAJOR << 16) | \
     (DB_VERSION_MINOR << 8) | \
     (DB_VERSION_PATCH))

/*****************************************************************************/

/*
 * Benchmarking code utilized by an outside test program.  This hardcodes a
 * few behaviours specific to benchmarking the code and not worrying about
 * actively serving multiple real users.  This switch does _NOT_ cause the
 * test suite to be executed.  See OPENFT_TEST_SUITE in ft_openft.c for that.
 */
/* #define SEARCH_DB_BENCHMARK */

/*
 * Primitive interactive querying when using the test suite.
 */
/* #define INTERACTIVE */

/* 
 * Whenever a host is removed, check the entire tokens index to ensure
 * there are no stray tokens left from that host. Expensive, use with
 * caution.
 */
/* #define PARANOID */

/* be verbose about single removals in an attempt to track down greycat's bug */
/* #define DEBUG_REMOVE */

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
 * It has been determined that this causes a great deal of problems in
 * DB 4.1 and presumably 4.0.  Once you create a database with a
 * custom hash function in a file containing multiple databases, it
 * seems you no longer have the ability to remove it, even if you
 * reset the hash function prior to calling DB->remove.  It appears as
 * though DB 3.x is unaffected, but I have no further information than
 * that.
 *
 * UPDATE:
 *
 * We have discovered a work-around to this problem that is documented in
 * direct_md5_hash.
 *
 * UPDATE 2:
 *
 * The workaround is currently not required since the share data was
 * moved into a single database.
 */
#define SEARCH_DB_DIRHASH

/*****************************************************************************/

/*
 * How often to perform remove requests.  For each removal request, we will
 * remove at most `SEARCH_DB_REMOVE_COUNT' entries from the database and then
 * return to the event loop to avoid choking the main program.
 */
#define SEARCH_DB_REMOVE_INTERVAL ((100) * MSEC)

/*
 * Controls how many entries to remove during a single remove request.  This
 * refers to each individual database record found in the per-user
 * share.index, and this parameter applies only when we are destroying an
 * entire set of shares from a specific user.
 */
#define SEARCH_DB_REMOVE_COUNT (30)

/*****************************************************************************/

/*
 * Split per-share token lists out into separate files - a Good Idea (TM)
 * because libdb's subdatabase handling seems rather space-inefficient.
 * Perhaps libdb 4.2 improves this?
 */
#define SEARCH_DB_SEPARATE_INDICES

/*****************************************************************************/

#define DB_BITS 12
#define ID_BITS 20

/* this must be larger than the search/children config key! */
#define MAX_CHILDREN   (1 << DB_BITS) 

/* local_child is always the first db created, so it gets id zero */
#define LOCAL_CHILD    0

/*
 * All of the globally opened environment and database handlers go here.
 * Keep in mind that the actual share record data goes in a separate database
 * per user sharing the files, but all databases are opened within the same
 * file.
 */
static DB_ENV *env_search      = NULL;
static char   *env_search_path = NULL;
static DB     *db_md5_idx      = NULL;
static DB     *db_token_idx    = NULL;
static DB     *db_share_data   = NULL;
FTSearchDB    *local_child     = NULL;

/* NOTE: this is not actually a globally-unique ID, merely unique
 * per-child.  But to save tracking which IDs each child has used we
 * just keep a single counter and increment and retry when insertion
 * fails.
 */
static uint32_t global_id = 0;

#ifndef SEARCH_DB_BENCHMARK
static Array  *remove_queue    = NULL;
static BOOL    remove_active   = FALSE;
#endif /* SEARCH_DB_BENCHMARK */

/*****************************************************************************/

struct md5idx_key
{
	unsigned char md5[16];
};

struct md5idx_data
{
	FTSearchDB *sdb;
	uint32_t    id;
};

/*****************************************************************************/

struct tokenidx_key
{
	uint32_t token;                    /* single element of a search keyword
	                                    * or share token */
};

#ifdef _MSC_VER
# pragma pack(push,1)
#endif

struct tokenidx_data
{
#if 0
	FTSearchDB   *sdb;                 /* raw pointer into the database */
	uint32_t      id;
#else
	/* FIXME: evil limits here! 4096 total children, ~1 million
	 * total files (global) */
	uint32_t      db_id:DB_BITS;
	uint32_t      id:ID_BITS;
#endif
	unsigned char order;
	unsigned char count;
}
#ifdef __GNUC__
__attribute ((packed)); /* avoid alignment padding */
#endif
;

#ifdef _MSC_VER
# pragma pack(pop)
#endif

/*****************************************************************************/

struct sharedata_key
{
	FTSearchDB   *sdb;                 /* raw pointer into the database */
	uint32_t      id;
};

struct sharedata_data
{
	unsigned char  md5[16];            /* hash */
	off_t          size;               /* file size */
	u_int16_t      order;              /* offset of order list */
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
};

/*****************************************************************************/

struct shareidx_key
{
	unsigned char md5[16];
	uint32_t      id;
};

#if 0
struct shareidx_data
{
	uint32_t *tokens;
};
#endif

/*****************************************************************************/

#if DB_VERSION >= 0x030200
# define DB_COMPARE_HAS_DBP
#endif /* libdb >= 3.2.0 */

#ifdef DB_COMPARE_HAS_DBP
# define DB_COMPAREFN(cmpfn) int cmpfn (DB *dbp, const DBT *a, const DBT *b)
# define DB_HASHFN(hashfn) uint32_t hashfn (DB *dbp, const void *bytes, \
                                            u_int32_t length)
#else
# define DB_COMPAREFN(cmpfn) int cmpfn (const DBT *a, const DBT *b)
# define DB_HASHFN(hashfn) uint32_t hashfn (const void *bytes, u_int32_t length)
#endif

typedef int (*DBCompareFn) (
#ifdef DB_COMPARE_HAS_DBP
                            DB *dbp,
#endif
                            const DBT *a, const DBT *b);

/*****************************************************************************/

#define ERR_DB_NODE(method,ret,node)       \
	FT->DBGFN (FT, "%s: %s failed: %s",    \
	           ft_node_fmt(node), method, db_strerror(ret))

#define ERR_DB_SDB(method,ret,sdb)         \
	do {                                   \
		ERR_DB_NODE(method,ret,sdb->node); \
	} while (0)

#define ERR_DB_PATH(method,ret,path,db)    \
	FT->DBGFN (FT, "%s(%s:%s) failed: %s", method, path, STRING_NOTNULL(db), \
	           db_strerror(ret))

#define ERR_DB(method,ret)                 \
	FT->DBGFN (FT, "%s failed: %s", method, db_strerror(ret))

/*****************************************************************************/

static BOOL db_remove_host_timer (FTSearchDB *sdb);
static BOOL db_remove_sharedata (FTSearchDB *sdb, uint32_t id);
static BOOL db_remove_shareidx (FTSearchDB *sdb, unsigned char *md5, uint32_t id);
static uint32_t db_lookup_md5 (FTSearchDB *sdb, unsigned char *md5);
static Share *db_get_share (FTSearchDB *sdb, uint32_t id, uint8_t **order);
static BOOL db_close (FTSearchDB *sdb, BOOL rm);
static BOOL db_sync (FTSearchDB *sdb);
static BOOL db_abort (FTSearchDB *sdb);

/*****************************************************************************/

static FTSearchDB *child_index[MAX_CHILDREN];
static int last_child_id, child_count;

/* returns a unique child ID */
static int child_new (FTSearchDB *sdb)
{
	int id = last_child_id;

	assert (sdb);

	/* FIXME: the count includes children that have disconnected
	 * and are pending removal, so this can trigger in unfortunate
	 * situations where the removal queue is long despite the
	 * child count being clamped below the limit.  (Yes, this has
	 * happened to me.)
	*/
	child_count++;
	assert (child_count < MAX_CHILDREN);

	while (child_index[id])
		id++;

	child_index[id] = sdb;

	return id;
}

static void child_free (int id)
{
	assert (child_index[id]);

	child_count--;

	child_index[id] = NULL;
}

static FTSearchDB *child_lookup (int id)
{
	FTSearchDB *sdb;

	assert (id >= 0 && id < MAX_CHILDREN);

	sdb = child_index[id];

	assert (sdb);

	return sdb;
}

static void init_children (void)
{
	int i;
	for (i=0; i < MAX_CHILDREN; i++)
		child_index[i] = NULL;

	last_child_id = child_count = 0;
}

/*****************************************************************************/

static FTSearchDB *search_db_new (FTNode *node)
{
	FTSearchDB *sdb;

	if (!(sdb = MALLOC (sizeof (FTSearchDB))))
		return NULL;

	sdb->share_idx_name = NULL;
	sdb->share_idx = NULL;
	sdb->remove_curs = NULL;
	sdb->shares = 0;
	sdb->size = 0;
#ifdef BLOOM
	sdb->filter = sdb->old_filter = NULL;
#endif

	if (node)
		node->session->search_db = sdb;
	
	sdb->node = node;

	sdb->id = child_new (sdb);

	FT->DBGFN (FT, "db_new: %s (%p) has id %d (0x%x)", ft_node_fmt (node),
		   sdb, sdb->id, sdb->id);

	return sdb;
}

static void search_db_free (FTSearchDB *sdb)
{
	assert (sdb != NULL);
	assert (sdb->share_idx == NULL);
	assert (sdb->remove_curs == NULL);

	/*
	 * Check to make sure the search database from the session is NULL or was
	 * assigned to a new search database.  This should have been guaranteed
	 * by ::ft_search_db_remove_host, but it doesn't hurt to make sure here.
	 *
	 * NOTE: This is also set to NULL by ft_session.c:ft_session_stop.
	 */
	if (sdb->node && sdb->node->session)
		assert (sdb->node->session->search_db != sdb);

#ifdef BLOOM
	assert (sdb->filter == NULL);
	assert (sdb->old_filter == NULL);
#endif

	child_free (sdb->id);

	FT->DBGFN (FT, "db_free: freed %p with id %d (0x%x)", sdb, sdb->id, sdb->id);

	free (sdb->share_idx_name);
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

#ifndef SEARCH_DB_BENCHMARK
	FT->DBGFN (FT, "opened(%i) %p %s:%s", ret, dbp, path,
	           STRING_NOTNULL(database));
#endif /* !SEARCH_DB_BENCHMARK */

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

#ifndef SEARCH_DB_BENCHMARK
	FT->DBGFN (FT, "attempting to remove %s:%s",
	           path, STRING_NOTNULL(database));
#endif /* !SEARCH_DB_BENCHMARK */

#if DB_VERSION >= 0x040100 && 0
	if ((ret = env_search->dbremove (env_search, NULL, path, database, 0)))
	{
		ERR_DB_PATH("DB_ENV->dbremove", ret, path, database);
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

static int close_db (DB *dbp, char *path, char *database, BOOL rm)
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

#ifndef SEARCH_DB_BENCHMARK
	FT->DBGFN (FT, "closing %p %s:%s(%i,%i)",
	           dbp, path, STRING_NOTNULL(database), rm, (int)flags);
#endif /* !SEARCH_DB_BENCHMARK */

	if ((ret = dbp->close (dbp, flags)))
	{
		ERR_DB_PATH("DB->close", ret, path, database);
		return ret;
	}

	if (rm)
		remove_db (env_search, path, database);

	assert (ret == 0);
	return ret;
}

static int allow_dups (DB *dbp, DBCompareFn cmpfunc, uint32_t pagesize)
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

	if (pagesize)
	{
		if ((ret = dbp->set_pagesize (dbp, pagesize)))
			ERR_DB("DB->set_pagesize", ret);
	}

	return TRUE;
}

static BOOL delete_key_data (DBC *dbcp, DBT *key, DBT *data)
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

/*****************************************************************************/

#ifdef SEARCH_DB_DIRHASH
static DB_HASHFN(direct_md5_hash)
{
	u_int32_t hash = 0;

	/*
	 * Due to a ridiculous libdb bug regarding multiple databases in a single
	 * database file with a custom hashing function, we need to fake the
	 * integrity test by pre-hashing "%$sniglet^&" with the default hash
	 * routine and returning it here.
	 */
	if (length == 12)
	{
		assert (strcmp (bytes, "%$sniglet^&") == 0);
		return 0x5e688dd1;
	}

#if 0
	/* make sure libdb isn't playing tricks on us */
	assert (length == 16);
#endif

	/* directly copy the leading byte of the MD5SUM */
	memcpy (&hash, bytes, sizeof (hash));
	return hash;
}
#endif /* SEARCH_DB_DIRHASH */

static DB *open_db_sharedata (void)
{
	DB *dbp = NULL;
	int ret;

	if (db_create (&dbp, env_search, 0) || !dbp)
		return NULL;

#if 0
	if ((ret = dbp->set_pagesize (dbp, 1024)))
		ERR_DB("DB->set_pagesize", ret);
#endif

	if ((ret = open_db (dbp, "share.data", NULL, DB_BTREE, DB_CREATE, 0664)))
	{
		close_db (dbp, "share.data", NULL, TRUE);
		return NULL;
	}

	return dbp;
}

static DB *db_sharedata (void)
{
	if (!db_share_data)
		db_share_data = open_db_sharedata();

	return db_share_data;
}

static char *db_shareidx_path (FTSearchDB *sdb, char **dbname)
{
	/*
	 * First time opening this database we need to generate a pseudo-unique
	 * name for the database so that it won't collide with any temporary
	 * databases being used at removal right now.
	 */
	if (!sdb->share_idx_name)
	{
		const char *idxfmt;

#ifdef SEARCH_DB_SEPARATE_INDICES
		idxfmt = "share.index-%s-%u";
#else /* !SEARCH_DB_SEPARATE_INDICES */
		idxfmt = "%s-%u";
#endif /* SEARCH_DB_SEPARATE_INDICES */

		/* prefix filenames so they're easily identifiable later */
		sdb->share_idx_name =
		    stringf_dup (idxfmt, net_ip_str (sdb->node->ninfo.host),
		                 (unsigned int)(time(NULL)));
	}

#ifdef SEARCH_DB_SEPARATE_INDICES
	if (dbname)
		*dbname = NULL;

	return sdb->share_idx_name;
#else /* !SEARCH_DB_SEPARATE_INDICES */
	/*
	 * We are actually opening multiple databases within one file here so we
	 * need to specify a unique database name to assist libdb in figuring out
	 * wtf we are talking about.
	 */
	if (dbname)
		*dbname = sdb->share_idx_name;

	return "share.index";
#endif /* SEARCH_DB_SEPARATE_INDICES */
}

static DB *open_db_shareidx (FTSearchDB *sdb)
{
	DB   *dbp = NULL;
	char *path;
	char *dbname;
	int   ret;

	if (!(path = db_shareidx_path (sdb, &dbname)))
		return NULL;

	if (db_create (&dbp, env_search, 0) || !dbp)
		return NULL;

#ifdef SEARCH_DB_DIRHASH
	/*
	 * The underlying database will be using MD5SUM sums as the distinct
	 * keys, and should opt instead to use the distribution of the actual
	 * data as the key for the hash table itself.  For this reason, we will
	 * be registering a custom hash function which simply uses the first 4
	 * bytes of the original data key.
	 */
	if ((ret = dbp->set_h_hash (dbp, direct_md5_hash)))
		ERR_DB_PATH("DB->set_h_hash", ret, path, dbname);
#endif /* SEARCH_DB_DIRHASH */

	/*
	 * Pagesize and h_ffactor are being experimented with to improve
	 * performance and space efficiency.  These settings still need to be
	 * tweaked.
	 */
#if 0
	if ((ret = dbp->set_pagesize (dbp, 1024)))
		ERR_DB_PATH("DB->set_pagesize", ret, path, dbname);
#endif

	if ((ret = dbp->set_h_ffactor (dbp, 45)))
		ERR_DB_PATH("DB->set_h_ffactor", ret, path, dbname);

	if ((ret = open_db (dbp, path, dbname, DB_HASH, DB_CREATE, 0664)) != 0)
	{
		close_db (dbp, path, dbname, TRUE);
		return NULL;
	}

	return dbp;
}

static DB *db_shareidx (FTSearchDB *sdb, BOOL allow_open)
{
	if (!sdb)
		return NULL;

	if (!sdb->share_idx)
	{
		if (!allow_open)
			db_abort (sdb);

		sdb->share_idx = open_db_shareidx (sdb);
	}

	return sdb->share_idx;
}

static DB *db_master (DB **dbpp, DBCompareFn cmpfunc, uint32_t pagesize,
		      char *path, DBTYPE type)
{
	DB *dbp = NULL;

	/* already opened */
	if (*dbpp)
		return *dbpp;

	if (db_create (&dbp, env_search, 0) || !dbp)
		return NULL;

	allow_dups (dbp, cmpfunc, pagesize);

	if (open_db (dbp, path, NULL, type, DB_CREATE, 0644) != 0)
		close_db (dbp, path, NULL, TRUE);
	else
		*dbpp = dbp;

	return *dbpp;
}

static DB_COMPAREFN (compare_sdb)
{
	static struct md5idx_data *a_rec;
	static struct md5idx_data *b_rec;
	int ret;

	a_rec = a->data;
	b_rec = b->data;

	assert (a->size == sizeof (*a_rec));
	assert (b->size == a->size);

	ret = memcmp (&a_rec->sdb, &b_rec->sdb, sizeof (a_rec->sdb));

	if (ret)
		return ret;
	
	return memcmp (&a_rec->id, &b_rec->id, sizeof (a_rec->id));
}

static DB *db_md5idx (void)
{
	DB *dbp;

	dbp = db_master (&db_md5_idx, compare_sdb, 0, "md5.index", DB_BTREE);

	return dbp;
}

static DB_COMPAREFN (compare_md5)
{
	static struct tokenidx_data a_rec, b_rec;
	int ret;

	assert (a->size == sizeof (a_rec));
	assert (b->size == a->size);

	/* We need to copy the structs because libdb doesn't guarantee
	 * alignedness.  Neither can we use memcmp (despite not caring
	 * about the relative ordering, only the uniqueness), as the
	 * fields aren't even byte-aligned.
	 */
	a_rec = *(struct tokenidx_data *)a->data;
	b_rec = *(struct tokenidx_data *)b->data;

	/* first compare id */
	if ((ret = a_rec.id - b_rec.id))
		return ret;

	/* then compare host */
	return a_rec.db_id - b_rec.db_id;

	/* the rest of the record is ignored, so that we don't have to
	 * worry about filling it in and it'll still match */
}

static DB *db_tokenidx (void)
{
	DB *dbp;

	/* secondary database is insert sorted by MD5 */
	dbp = db_master (&db_token_idx, compare_md5, 1024, "tokens.index", DB_BTREE);

	return dbp;
}

/*****************************************************************************/

/* this function avoids using file_rmdir as some users may have setup a
 * symlink for this directory and we dont want to damage it */
static void clean_db_path (const char *path)
{
	DIR *dir;
	struct dirent *d;

	/* shared db environment files (only if env_priv=0) */
	file_unlink (stringf ("%s/__db.001", path));
	file_unlink (stringf ("%s/__db.002", path));

	/* unlink our databases */
	file_unlink (stringf ("%s/share.data", path));
	file_unlink (stringf ("%s/share.index", path));
	file_unlink (stringf ("%s/md5.index", path));
	file_unlink (stringf ("%s/tokens.index", path));

	/* all the individual share indices, if we had that enabled */
	if ((dir = file_opendir (path)))
	{
		while ((d = file_readdir (dir)))
		{
			if (strncmp (d->d_name, "share.index-", 12) == 0)
				file_unlink (stringf ("%s/%s", path, d->d_name));
		}

		file_closedir (dir);
	}

	/* removed legacy databases */
	file_unlink (stringf ("%s/children.data", path));
}

/*
 * Initialize the database environment wherein all databases for the search
 * node reside.  Really the only reason we are doing this is to share an
 * underlying memory cache so that we dont need to guess the distribution of
 * the cache across the three core databases.
 */
static BOOL db_init (const char *envpath, u_int32_t cachesize)
{
	int       ret;
	u_int32_t flags = 0;
	u_int32_t c_gbytes;
	u_int32_t c_bytes;

	assert (env_search == NULL);

	/* we aren't checking the return value here because file_create_path
	 * sucks and needs to be rewritten :( */
	if (!(file_mkdir (envpath, 0755)))
	{
		FT->err (FT, "unable to mkdir %s: %s", envpath, GIFT_STRERROR());
		return FALSE;
	}

	if ((ret = db_env_create (&env_search, 0)))
	{
		ERR_DB("db_env_create", ret);
		return FALSE;
	}

	/* set the cache size for the entire environment */
	c_gbytes = 0;
	c_bytes  = cachesize;

	/* dump all the info to the log in case we can't get a gdb session
	 * from a user having troubles... */
	FT->DBGFN (FT, "search params: "
		   "libdb=%d.%d.%d, "
	           "path=%s, cache=%lu, "
	           "minpeers=%d, maxpeers=%d, "
	           "nchildren=%d, maxttl=%d, maxresults=%d",
		   DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
	           envpath, (unsigned long)c_bytes,
	           FT_CFG_SEARCH_MINPEERS, FT_CFG_SEARCH_MAXPEERS,
	           FT_CFG_MAX_CHILDREN, FT_CFG_SEARCH_TTL, FT_CFG_SEARCH_RESULTS);

#ifdef SEARCH_DB_BENCHMARK
#ifndef OPENFT_TEST_SUITE
	FT->DBGFN (FT, "BENCHMARKING ON!");
#endif
#endif

	if ((ret = env_search->set_cachesize (env_search, c_gbytes, c_bytes, 0)))
	{
		ERR_DB("DB_ENV->set_cachesize", ret);
		return FALSE;
	}

	/* initialize the memory pool subsystem, make the database private, and
	 * specify that any necessary files should be created for us */
	flags = DB_INIT_MPOOL | DB_CREATE;

	if (FT_CFG_SEARCH_ENV_TXN)
		flags |= DB_INIT_TXN | DB_INIT_LOG;

	if (FT_CFG_SEARCH_ENV_PRIV)
		flags |= DB_PRIVATE;

	if ((ret = env_search->open (env_search, envpath, flags, 0644)))
	{
		ERR_DB("DB_ENV->open", ret);
		return FALSE;
	}

#ifndef SEARCH_DB_BENCHMARK
	if (!(remove_queue = array_new (NULL)))
		return FALSE;
#endif

	return TRUE;
}

static void db_destroy (const char *envpath)
{
	assert (env_search != NULL);

	env_search->close (env_search, 0);
	env_search = NULL;
	
	clean_db_path (envpath);
}

/*****************************************************************************/

static u_int16_t serialize_fld (struct sharedata_data *datarec,
                                unsigned char *fld, size_t len)
{
	u_int16_t start;
	char      nul = 0;

	/* we must write at least one byte, make it a NUL */
	if (len == 0)
	{
		fld = &nul;
		len = sizeof (nul);
	}

	start = datarec->data_len;

	if (datarec->data_len + len >= sizeof (datarec->data))
		return start;

	memcpy (datarec->data + datarec->data_len, fld, len);
	datarec->data_len += len;

	return start;
}

static void sharedata_meta (ds_data_t *key, ds_data_t *value,
                            struct sharedata_data *datarec)
{
	assert (key->len > 0);
	assert (value->len > 0);

	serialize_fld (datarec, key->data, key->len);
	serialize_fld (datarec, value->data, value->len);
}

static int serialize_record (DBT *data, Hash *hash, struct tokenized *t,
                             Share *share)
{
	static struct sharedata_data datarec;

	datarec.data_len = 0;

	/* copy the staticly sized fields */
	datarec.size = share->size;
	memcpy (datarec.md5, hash->data, sizeof(datarec.md5));

	/* copy the variably sized fields */
	datarec.order = serialize_fld (&datarec, t->order, t->ordlen);
	datarec.path  = serialize_fld (&datarec, share->path, STRLEN_0(share->path));
	datarec.mime  = serialize_fld (&datarec, share->mime, STRLEN_0(share->mime));

	datarec.meta = datarec.data_len;
	share_foreach_meta (share, DS_FOREACH(sharedata_meta), &datarec);

	/* apply some quazi-portable magic */
	data->data = &datarec;
	data->size = sizeof (datarec) - (sizeof (datarec.data) - datarec.data_len);

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
                                  struct sharedata_data *datarec, uint8_t **order)
{
	Share *share;

	if (!sdb || !datarec)
		return NULL;
	
#ifdef SEARCH_DB_BENCHMARK
	/* ugly hack: md5 is unused for everything except the test
	 * suite, which still uses the old db format */
	if (!md5)
		md5 = datarec->md5;
	else
		datarec = (struct sharedata_data *)(((void *)datarec) - 16);
#else
	md5 = datarec->md5;
#endif

	share = ft_share_new (sdb->node, datarec->size, md5,
	                      datarec->data + datarec->mime,
	                      datarec->data + datarec->path);

	if (!share)
		return NULL;

	import_meta (share,
	             datarec->data + datarec->meta,
	             datarec->data_len - datarec->meta);

	if (order)
		*order = STRDUP (datarec->data + datarec->order);

	return share;
}

/*****************************************************************************/

static BOOL db_insert_shareidx (FTSearchDB *sdb, Hash *hash, struct tokenized *t, uint32_t id)
{
	static struct shareidx_key  keyrec;
#if 0
	static struct shareidx_data datarec;
#endif
	DB *dbp;
	DBT key;
	DBT data;
	int ret;
	int tokens_len;

	/* we don't track this for local shares, but pretend that we do */
	if (sdb == local_child)
		return TRUE;

	if (!(dbp = db_shareidx (sdb, FALSE)))
		return FALSE;

	assert (sizeof (keyrec.md5) == hash->len);
	memcpy (keyrec.md5, hash->data, sizeof (keyrec.md5));
	keyrec.id = id;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = &keyrec;
	key.size = sizeof (keyrec);

	/* determine the total number of non-zero token elements exist in this
	 * list so that we may pass the total size of the data block to libdb */
	tokens_len = t->len;

	data.data = t->tokens;
	data.size = t->len * (sizeof (*t->tokens));

	if ((ret = dbp->put (dbp, NULL, &key, &data, DB_NOOVERWRITE)))
	{
		ERR_DB_SDB("DB->put", ret, sdb);
		return FALSE;
	}

	return TRUE;
}

static uint32_t db_insert_sharedata (FTSearchDB *sdb, Hash *hash,
                                 struct tokenized *t, Share *share)
{
	static struct sharedata_key keyrec;
	DB *dbp;
	DBT key;
	DBT data;
	int ret;
	uint32_t id;
	int looped = 0;

	/* access the appropriate database, opening if necessary */
	if (!(dbp = db_sharedata()))
	{
		db_abort (sdb);
		return 0;
	}

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));
		
	key.data = &keyrec;
	key.size = sizeof (keyrec);

	if (sdb == local_child)
	{
		/* for local shares just store a pointer,
		 * as we have the share in memory anyway */
		data.data = &share;
		data.size = sizeof (share);

		/* TODO: order */

		/* not reffing this should be safe... I hope */
#if 0
		ft_share_ref (share);
#endif
	}
	else
	{
		/* get the complete serialized record */
		if (!serialize_record (&data, hash, t, share))
			return 0;
	}

	/* choose a new id, retrying until it succeeds */
	do {
		/* skip zero */
		id = global_id = (global_id + 1) & ((1<<ID_BITS)-1);
		if (!id)
		{
			id = ++global_id;

			/* abort if we have too many shares */
			if (looped++)
			{
				db_abort (sdb);
				return 0;
			}
		}

		keyrec.sdb = sdb;
		keyrec.id  = id;

		/* insert a unique entry, resulting in an error if data already exists at
		 * this key */
		if ((ret = dbp->put (dbp, NULL, &key, &data, DB_NOOVERWRITE)))
			assert (ret == DB_KEYEXIST);
	} while (ret);

	return id;
}

static BOOL db_insert_md5idx (FTSearchDB *sdb, Hash *hash, struct tokenized *t,
                              Share *share, uint32_t id)
{
	static struct md5idx_key  keyrec;
	static struct md5idx_data datarec;
	DB *dbp;
	DBT key;
	DBT data;
	int ret;

	/* just point a simple reference to that database in the master primary */
	if (!(dbp = db_md5idx()))
		return FALSE;

	assert (hash->len == sizeof (keyrec.md5));
	memcpy (keyrec.md5, hash->data, hash->len);

	datarec.sdb = sdb;
	datarec.id  = id;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data  = &keyrec;
	key.size  = sizeof (keyrec);
	data.data = &datarec;
	data.size = sizeof (datarec);

	if ((ret = dbp->put (dbp, NULL, &key, &data, DB_NODUPDATA)))
	{
		ERR_DB_SDB("DB->put", ret, sdb);
		return FALSE;
	}

	return TRUE;
}

static BOOL db_insert_tokenidx (FTSearchDB *sdb, Hash *hash, struct tokenized *t,
                                Share *share, uint32_t id)
{
	static struct tokenidx_key  keyrec;
	static struct tokenidx_data datarec;
	DB  *dbp;
	DBC *dbcp;
	DBT  key;
	DBT  data;
	int  ret;
	int  i;

	if (!(dbp = db_tokenidx()))
		return FALSE;

	/* create a cursor because it seems we cant insert sorted without
	 * one...hmm, libdb is quirky */
	if (dbp->cursor (dbp, NULL, &dbcp, 0))
		return FALSE;

	datarec.db_id = sdb->id;
	datarec.id  = id;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = &keyrec;
	key.size = sizeof (keyrec);
	data.data = &datarec;
	data.size = sizeof (datarec);

	/* insert one record for each token in the stream, effectively
	 * "pre-searching" for this file */
	for (i=0; i < t->len; i++)
	{
		/* key.data points to &keyrec, so this is actually going to modify
		 * the data libdb is seeing */
		keyrec.token = t->tokens[i];
		datarec.order = i + ORDER_MIN;
		datarec.count = t->count[i];

		if ((ret = dbcp->c_put (dbcp, &key, &data, DB_KEYFIRST)))
		{
			if (ret != DB_KEYFIRST)
				ERR_DB_SDB("DBcursor->c_put", ret, sdb);
		}
	}

	dbcp->c_close (dbcp);

	/* TODO: track individual c_put errors? */
	return TRUE;
}

static BOOL db_insert (FTSearchDB *sdb, Share *share)
{
	Hash     *hash;
	struct tokenized *t;
	BOOL      success;
	uint32_t  id;

	/* make sure the master databases are open and ready to go */
	if (!db_md5idx() || !db_tokenidx())
		return FALSE;

	if (!(hash = share_get_hash (share, "MD5")))
		return FALSE;

	if (!(t = ft_tokenize_share (share, TOKENIZE_COUNT | TOKENIZE_ORDER)))
		return FALSE;

	success = TRUE;

	/*
	 * Insert into the global primary and secondary databases, as well as the
	 * host-specific primary database.  See ft_search_db.h for more details
	 * on exactly how this stuff is designed, if you're willing to believe it
	 * was designed at all :)
	 */
	if ((id = db_insert_sharedata (sdb, hash, t, share)))
	{
		success = db_insert_shareidx (sdb, hash, t, id);
		assert (success == TRUE);

		/*
		 * TODO: Handle db_insert_tokenidx failure (the flow control is going
		 * to get messy and require decomposition to functions, surely).
		 */
		if ((success = db_insert_md5idx (sdb, hash, t, share, id)))
			db_insert_tokenidx (sdb, hash, t, share, id);
		else
		{
			FT->DBGFN (FT, "rolling back db_insert_sharedata");

			/*
			 * Uh oh, abort the process and undo our changes to the db.  Hmm,
			 * should we perhaps use a libdb feature for this...it would
			 * certainly be able to do a better job...
			 */
			if (!(db_remove_sharedata (sdb, id)))
				db_abort (sdb);

			if (!(db_remove_shareidx (sdb, hash->data, id)))
				db_abort (sdb);
		}
	}
	else
		db_abort (sdb);

	ft_tokenize_free (t);

#ifdef BLOOM
	if (success)
		ft_bloom_add (sdb->filter, hash->data);
#endif

	return success;
}

/*****************************************************************************/

static BOOL db_remove_tokenidx_token (DBC *dbcp, FTSearchDB *sdb,
                                      uint32_t id, uint32_t token)
{
	static struct tokenidx_key  keyrec;
	static struct tokenidx_data datarec;
	DBT key;
	DBT data;

	keyrec.token = token;

	datarec.db_id = sdb->id;
	datarec.id  = id;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = &keyrec;
	key.size = sizeof (keyrec);
	data.data = &datarec;
	data.size = sizeof (datarec);

	return delete_key_data (dbcp, &key, &data);
}

static BOOL db_remove_tokenidx (FTSearchDB *sdb,
                                struct tokenized *t, uint32_t id)
{
	DB  *dbp;
	DBC *dbcp;
	BOOL ret = TRUE;
	int  i;

	if (!(dbp = db_tokenidx()))
		return FALSE;

	/* construct a cursor so that we may use it for removing all tokens
	 * at once */
	if (dbp->cursor (dbp, NULL, &dbcp, 0))
		return FALSE;

	for (i=0; i < t->len; i++)
	{
		if (!(ret = db_remove_tokenidx_token (dbcp, sdb, id, t->tokens[i])))
		{
			FT->DBGFN (FT, "%p(node=%s): tok=%d (%x)",
			           sdb, ft_node_fmt (sdb->node),
			           (int)(t->tokens[i]), (unsigned int)(t->tokens[i]));
			db_abort (sdb);
			break;
		}
	}

	dbcp->c_close (dbcp);
	return ret;
}

static BOOL db_remove_sharedata (FTSearchDB *sdb, uint32_t id)
{
	static struct sharedata_key keyrec;
	DB *dbp;
	DBT key;
	int ret;

	if (!(dbp = db_sharedata()))
		return FALSE;

	keyrec.sdb = sdb;
	keyrec.id  = id;

	memset (&key, 0, sizeof (key));
	key.data = &keyrec;
	key.size = sizeof (keyrec);

	/* we don't have to be host-aware when removing from this database, as
	 * it is isolated away from the master */
	if ((ret = dbp->del (dbp, NULL, &key, 0)))
	{
		ERR_DB_SDB("DB->del", ret, sdb);
		return FALSE;
	}

	return TRUE;
}

static BOOL db_remove_shareidx (FTSearchDB *sdb, unsigned char *md5, uint32_t id)
{
	static struct shareidx_key keyrec;
	DB *dbp;
	DBT key;
	int ret;

	/* we don't track this for local shares, but pretend that we do */
	if (sdb == local_child)
		return TRUE;

	if (!(dbp = db_shareidx (sdb, FALSE)))
		return FALSE;

	assert (sizeof (keyrec.md5) == 16);
	memcpy (keyrec.md5, md5, sizeof (keyrec.md5));

	keyrec.id = id;

	memset (&key, 0, sizeof (key));
	key.data = &keyrec;
	key.size = sizeof (keyrec);

	if ((ret = dbp->del (dbp, NULL, &key, 0)))
	{
		ERR_DB_SDB("DB->del", ret, sdb);
		return FALSE;
	}

	return TRUE;
}

static BOOL db_remove_md5idx (FTSearchDB *sdb, unsigned char *md5, uint32_t id)
{
	static struct md5idx_key  keyrec;
	static struct md5idx_data datarec;
	DB  *dbp;
	DBC *dbcp;
	DBT  key;
	DBT  data;
	int  ret;

	if (!(dbp = db_md5idx()))
		return FALSE;

	if (dbp->cursor (dbp, NULL, &dbcp, 0))
		return FALSE;

	assert (sizeof (keyrec.md5) == 16);
	memcpy (keyrec.md5, md5, sizeof (keyrec.md5));

	datarec.sdb = sdb;
	datarec.id  = id;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = &keyrec;
	key.size = sizeof (keyrec);
	data.data = &datarec;
	data.size = sizeof (datarec);

	ret = delete_key_data (dbcp, &key, &data);

	dbcp->c_close (dbcp);

	return ret;
}

/*
 * Remove a single share given its unique id.  This is used primarily
 * when syncing smaller changes to the share database without a full dump and
 * reimport.  This is not used internally with db_remove_host, which does
 * something significantly more efficient for massive removal.
 */
static BOOL db_remove (FTSearchDB *sdb, uint32_t id, off_t *retsize)
{
	Share    *share;
	struct tokenized *t;
	unsigned char *md5;
	BOOL      failure;

	if (!db_md5idx() || !db_tokenidx())
		return FALSE;

#ifdef DEBUG_REMOVE
	/* evil, evil debugging hackery */
	if (!(share = db_get_share (sdb, id, 1)))
#else
	if (!(share = db_get_share (sdb, id, NULL)))
#endif
		db_abort (sdb);

	md5 = share_get_hash (share, "MD5")->data;

	/* so that the caller knows how large the entry was that was just
	 * removed (for stats purposes) */
	if (retsize)
		*retsize = share->size;

	/*
	 * Attempt to remove the entry completely.  First begin with the primary
	 * databases, then move onto the secondary token index.
	 *
	 * Note that we track failure for the retval of this func, but keep
	 * drudging on as long as possible just to try to tidy up the database
	 * as much as possible.  Perhaps this is unwise?
	 */
	failure = FALSE;

	if (!db_remove_md5idx (sdb, md5, id))
	{
		FT->DBGFN (FT, "%s: remove_md5idx failed for '%s'", 
			   ft_node_fmt (sdb->node), md5_fmt (md5));
		failure = TRUE;
	}

	if (!db_remove_sharedata (sdb, id))
	{
		FT->DBGFN (FT, "%s: remove_sharedata failed for '%s'", 
			   ft_node_fmt (sdb->node), md5_fmt (md5));
		failure = TRUE;
	}

	if (!db_remove_shareidx  (sdb, md5, id))
	{
		FT->DBGFN (FT, "%s: remove_shareidx failed for '%s'", 
			   ft_node_fmt (sdb->node), md5_fmt (md5));
		failure = TRUE;
	}

	/* tokenize so that we know exactly what we're supposed to be removing
	 * from the secondary database */
	if (!(t = ft_tokenize_share (share, 0)))
		db_abort (sdb);

	/* attempt to remove each token individually from the secondary
	 * token index */
	if (!db_remove_tokenidx (sdb, t, id))
	{
		FT->DBGFN (FT, "%s: remove_tokenidx failed for '%s'", 
			   ft_node_fmt (sdb->node), md5_fmt (md5));
		failure = TRUE;
	}
#ifdef DEBUG_REMOVE
	else
	{
		FT->DBGFN (FT, "%s: removing %08x:%08x '%s', ntok=%d", ft_node_fmt (sdb->node),
			   sdb, id, md5_fmt (md5), t->len);
		GIFT_TRACEMEM (t->tokens, t->len * sizeof(*t->tokens));
	}
#endif

	ft_tokenize_free (t);

	/* we only needed this for the token list and size, sigh */
	if (ft_share_unref (share) > 0)
		assert (sdb == local_child);

	return !failure;
}

#ifdef PARANOID
static void check_tokens (FTSearchDB *sdb)
{
	DB  *dbp;
	DBC *dbcp;
	int  ret;
	DBT  key;
	DBT  data;
	struct tokenidx_key *keyrec;
	struct tokenidx_data *datarec;
	int  count = 0;
	int  id = sdb->id;

	if (!(dbp = db_tokenidx()))
		db_abort (sdb);
	
	FT->DBGFN (FT, "checking tokens idx after removing %p", sdb);

	if ((ret = dbp->cursor (dbp, NULL, &dbcp, 0)) || !dbcp)
	{
		ERR_DB_SDB("DB->cursor", ret, sdb);
		return;
	}

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	while (!(ret = dbcp->c_get (dbcp, &key, &data, DB_NEXT)))
	{
		assert (key.size == sizeof (*keyrec));
		assert (data.size == sizeof (*datarec));

		keyrec = key.data;
		datarec = data.data;
		
		if (datarec->db_id == id ||
		    !child_index[datarec->db_id])
			db_abort (sdb);

		count++;
	}

	if (ret != DB_NOTFOUND)
	{
		ERR_DB_SDB("DBcursor->c_get", ret, sdb);
		db_abort (sdb);
	}

	if ((ret = dbcp->c_close (dbcp)))
		ERR_DB_SDB("DBcursor->c_close", ret, sdb);

	FT->DBGFN (FT, "all done (%d tokens checked)", count);

}
#endif

/*****************************************************************************/

static BOOL db_remove_host_init (FTSearchDB *sdb)
{
	DB  *dbp;
	DBC *dbcp;
	int  ret;

	if (!(dbp = db_shareidx (sdb, TRUE)))
		return FALSE;

	if ((ret = dbp->cursor (dbp, NULL, &dbcp, 0)) || !dbcp)
	{
		ERR_DB_SDB("DB->cursor", ret, sdb);
		return FALSE;
	}

	sdb->remove_curs = dbcp;

	return TRUE;
}

static void db_remove_host_finish (FTSearchDB *sdb)
{
	DBC *dbcp;
	int  ret;

#ifndef SEARCH_DB_BENCHMARK
	FT->DBGFN (FT, "%s: removed %lu shares",
	           sdb->share_idx_name, sdb->shares);
#endif /* SEARCH_DB_BENCHMARK */

	if ((dbcp = sdb->remove_curs))
	{
		if ((ret = dbcp->c_close (dbcp)))
			ERR_DB_SDB("DBcursor->c_close", ret, sdb);

		sdb->remove_curs = NULL;
	}

#ifdef PARANOID
	check_tokens (sdb);
#endif

	/* close and remove db_shareidx */
	db_close (sdb, TRUE);

	/* clean up the search database handle allocated to the node structure */
	search_db_free (sdb);
}

static BOOL db_remove_host_next (FTSearchDB *sdb)
{
	static struct shareidx_key  *keyrec;
#if 0
	static struct shareidx_data *datarec;
#else
	static uint32_t             *datarec;
#endif
	DB  *dbp;
	DBC *dbcp;
	DBT  key;
	DBT  data;
	int  ret;
	int  i;

	assert (sdb->share_idx != NULL);
	assert (sdb->remove_curs != NULL);

	dbp = db_shareidx (sdb, FALSE);
	assert (dbp == sdb->share_idx);

	dbcp = sdb->remove_curs;
	assert (dbcp != NULL);

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	/*
	 * Perform at most `SEARCH_DB_REMOVE_COUNT' share removals.  We exist
	 * in a non-blocking event loop that can not be allowed to choke for
	 * long.
	 */
	for (i = 0; i < SEARCH_DB_REMOVE_COUNT; i++)
	{
		struct tokenized t;

		/*
		 * Access the record data from db_shareidx that we will need to
		 * use for removing from the main data and indices.  If this fails
		 * due to DB_NOTFOUND, we should inform the caller (with a FALSE
		 * return) that there are no shares left to remove.  The share
		 * index will be truncated and removed, and we can move on.
		 */
		if ((ret = dbcp->c_get (dbcp, &key, &data, DB_NEXT)))
		{
			assert (ret == DB_NOTFOUND);
			return FALSE;
		}

		assert (key.size == sizeof (*keyrec));
		keyrec = key.data;

		datarec = data.data;

		t.tokens = datarec;
		t.len = (data.size / sizeof (*t.tokens));
		
		/*
		 * Begin removing from the 3 other databases.
		 *
		 * TODO: Track, report, and handle errors during removal
		 *  from the other databases.  Any thoughts on what an
		 *  appropriate action would be in the face of errors here?
		 *
		 * TODO: Divide db_remove and call that here?
		 */
		db_remove_md5idx    (sdb, keyrec->md5, keyrec->id);
		db_remove_tokenidx  (sdb, &t, keyrec->id);
		db_remove_sharedata (sdb, keyrec->id);

		/*
		 * This code would otherwise be a call to db_remove_shareidx, but it
		 * is not required here simply because our purpose is to destroy this
		 * entire database.  Truncation and removal can occur in a single
		 * operation when we have finished removing each record described
		 * here from the other database indicdes.
		 */
#if 0
		if ((ret = dbcp->c_del (dbcp, 0)))
		{
			ERR_DB_SDB("DBcursors->c_del", ret, sdb);
			db_abort (sdb);
		}
#endif
	}

	/* indicate that there is more left to remove, and that we need more
	 * calls to remove it */
	return TRUE;
}

static BOOL db_remove_host_schedule (FTSearchDB *sdb)
{
	BOOL ret;

#ifndef SEARCH_DB_BENCHMARK
	FT->DBGFN (FT, "%s: scheduled removal (queued=%u)",
	           ft_node_fmt (sdb->node),
	           (unsigned int)(array_count (&remove_queue)));
#endif /* !SEARCH_DB_BENCHMARK */

	/*
	 * Nullify the node pointer as we do not want to appear attached to any
	 * particular node anymore.  This logic is used to prevent stale lookups
	 * elsewhere in the code.
	 */
	sdb->node = NULL;

#ifdef BLOOM
	if (sdb->old_filter)
		ft_bloom_unmerge (sdb->old_filter, md5_filter.filter);
	else
		if (sdb->filter)
			ft_bloom_unmerge (sdb->filter, md5_filter.filter);

	ft_bloom_free (sdb->filter);
	ft_bloom_free (sdb->old_filter);

	sdb->filter = sdb->old_filter = NULL;
#endif

	/*
	 * Initialize the cursor and other data objects necessary to perform
	 * the non-blocking removal.
	 */
	ret = db_remove_host_init (sdb);
	assert (ret == TRUE);

#ifndef SEARCH_DB_BENCHMARK
	/*
	 * We need to make sure that we are only processing one removal request
	 * at a time as they are extremely expensive and can cause serious
	 * performance problems if allowed to run simultaneously.
	 */
	if (remove_active == FALSE)
	{
		remove_active = TRUE;

		/*
		 * Remove up to `SEARCH_DB_REMOVE_COUNT' items every
		 * `SEARCH_DB_REMOVE_INTERVAL' milliseconds.
		 */
		timer_add (SEARCH_DB_REMOVE_INTERVAL,
		           (TimerCallback)db_remove_host_timer, sdb);
	}
	else
	{
		/*
		 * Schedule for removal later.  When `db_remove_host_timer' finishes,
		 * it will automatically initialize and begin handling this sdb.
		 */
		if (array_push (&remove_queue, sdb) == NULL)
			abort();
	}
#else /* SEARCH_DB_BENCHMARK */
	/*
	 * Block while each share is removed in an effort to get a better
	 * performance gauge.  There is little noise from the event loop involved
	 * in the benchmark code and this helps us gauge minor performance
	 * differences.
	 */
	while (db_remove_host_timer (sdb) == TRUE);
#endif /* !SEARCH_DB_BENCHMARK */

	return TRUE;
}

static FTSearchDB *db_remove_host_next_scheduled (void)
{
	FTSearchDB *next = NULL;

#ifndef SEARCH_DB_BENCHMARK
	/* process the remove queue (see db_remove_host_scheduled) */
	if ((next = array_shift (&remove_queue)))
	{
		FT->DBGFN (FT, "%u items remaining",
		           (unsigned int)(array_count (&remove_queue)));

		/*
		 * WARNING: This code copied from above to preserve flow control
		 * with SEARCH_DB_BENCHMARK.  Sigh.
		 */
		if (db_remove_host_init (next) == FALSE)
			abort();

		timer_add (SEARCH_DB_REMOVE_INTERVAL,
		           (TimerCallback)db_remove_host_timer, next);
	}
	else
	{
		/* all done */
		FT->DBGFN (FT, "remove queue empty");
		remove_active = FALSE;
	}
#endif /* !SEARCH_DB_BENCHMARK */

	return next;
}

static BOOL db_remove_host_timer (FTSearchDB *sdb)
{
	BOOL ret;

	/*
	 * Remove one entry, walking along a cursor initialized at the beginning
	 * of the db_shareidx database.  The return value here indicates whether
	 * or not we need to return here and call db_remove_host_next again to
	 * continue the removal process.  If FALSE, the removal process has
	 * completed (either successfully or in error), and we should tidy up.
	 */
	if ((ret = db_remove_host_next (sdb)) == FALSE)
	{
		db_remove_host_finish (sdb);
		db_remove_host_next_scheduled();

		/* nuke the timer */
		return FALSE;
	}

	/* keep the timer moving so long as we can successfully remove one element
	 * each call */
	return TRUE;
}

/*****************************************************************************/

static BOOL db_sync (FTSearchDB *sdb)
{
	DB *dbp;

	if ((dbp = db_sharedata()))
		dbp->sync (dbp, 0);

	if (sdb)
	{
		/* not going to call db_shareidx because it is designed to abort
		 * if used when the share index has not been previously opened */
		if ((dbp = sdb->share_idx))
			dbp->sync (dbp, 0);
	}

	if ((dbp = db_md5idx()))
		dbp->sync (dbp, 0);

	if ((dbp = db_tokenidx()))
		dbp->sync (dbp, 0);

	/* TODO: pass down errors */
	return TRUE;
}

static BOOL db_abort (FTSearchDB *sdb)
{
	BOOL ret;

	FT->DBGFN (FT, "fatal libdb error encountered, deploying parachute...");

	ret = db_sync (sdb);
	abort ();

	return ret;
}

/*****************************************************************************/

static BOOL db_close (FTSearchDB *sdb, BOOL rm)
{
	char *path;
	char *dbname;
	int   ret = 0;                     /* TODO: should be -1? */

	if (!sdb->share_idx)
		return TRUE;

	assert (sdb->remove_curs == NULL);

	if ((path = db_shareidx_path (sdb, &dbname)))
	{
		if ((ret = close_db (sdb->share_idx, path, dbname, rm)) == 0)
			sdb->share_idx = NULL;
	}

	return (ret == 0);
}

/*****************************************************************************/

static DBC *cursor_md5idx_md5 (DB *dbp, unsigned char *md5)
{
	static struct md5idx_key keyrec;
	DBC *dbcp;
	DBT  key;
	DBT  data;
	int  ret;

	if ((ret = dbp->cursor (dbp, NULL, &dbcp, 0)))
	{
		ERR_DB("DB->cursor", ret);
		return NULL;
	}

	assert (sizeof (keyrec.md5) == 16);
	memcpy (keyrec.md5, md5, sizeof (keyrec.md5));

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = &keyrec;
	key.size = sizeof (keyrec);

	/*
	 * Position the cursor at the beginning of the md5 set (containing share
	 * host ip addresses).  Unfortunately, we are actually reading the data
	 * element here and then simply throwing it away as I do not believe it
	 * is valid to request the cursor set without a data lookup.  Someone
	 * research this if you've got the time.
	 */
	if ((ret = dbcp->c_get (dbcp, &key, &data, DB_SET)))
	{
		assert (ret == DB_NOTFOUND);

		dbcp->c_close (dbcp);
		return NULL;
	}

	return dbcp;
}

/* this is only used for single share removal */
static uint32_t db_lookup_md5 (FTSearchDB *sdb, unsigned char *md5)
{
	static struct md5idx_data *datarec;
	DB       *dbp;
	DBC      *dbcp;
	DBT       key;
	DBT       data;
	u_int32_t flags;
	uint32_t  id = 0;

	if (!(dbp = db_md5idx()))
		return 0;

	if (!(dbcp = cursor_md5idx_md5 (dbp, md5)))
		return 0;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	/* iterate the cursor */
	for (flags = DB_CURRENT;
	     dbcp->c_get (dbcp, &key, &data, flags) == 0;
	     flags = DB_NEXT_DUP)
	{
		assert (data.size == sizeof (*datarec));
		datarec = data.data;

		/* ignore results from nodes currently being removed in the
		 * background */
		if (datarec->sdb == sdb)
		{
			id = datarec->id;
			break;
		}
	}
	
	dbcp->c_close (dbcp);

	return id;
}

/* ewww */
static uint32_t db_lookup_local_share (Share *share)
{
	static struct md5idx_data *datarec;
	unsigned char *md5;
	Hash     *hash;
	DB       *dbp;
	DBC      *dbcp;
	DBT       key;
	DBT       data;
	u_int32_t flags;
	uint32_t  id = 0;

	if (!(hash = share_get_hash (share, "MD5")))
		return 0;

	if (!(md5 = hash->data))
		return 0;

	if (!(dbp = db_md5idx()))
		return 0;

	if (!(dbcp = cursor_md5idx_md5 (dbp, md5)))
		return 0;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	/* iterate the cursor */
	for (flags = DB_CURRENT;
	     dbcp->c_get (dbcp, &key, &data, flags) == 0;
	     flags = DB_NEXT_DUP)
	{
		assert (data.size == sizeof (*datarec));
		datarec = data.data;

		/* ignore results from nodes currently being removed in the
		 * background */
		if (datarec->sdb == local_child)
		{
			Share *sh;

			sh = db_get_share (local_child, datarec->id, NULL);
			assert (sh);
			ft_share_unref (sh);

			if (sh == share)
			{
				id = datarec->id;
				break;
			}
		}
	}
	
	dbcp->c_close (dbcp);

	return id;
}

static Share *db_get_share (FTSearchDB *sdb, uint32_t id, uint8_t **order)
{
	static struct sharedata_key   keyrec;
	static struct sharedata_data *datarec;
	Share *share;
	DB    *dbp;
	DBT    key;
	DBT    data;
	int    ret;

	if (!(dbp = db_sharedata()))
		return FALSE;

	keyrec.sdb = sdb;
	keyrec.id  = id;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = &keyrec;
	key.size = sizeof (keyrec);

	if ((ret = dbp->get (dbp, NULL, &key, &data, 0)))
	{
		ERR_DB_SDB("DB->get", ret, sdb);

		db_abort (sdb);
	}

	if (sdb == local_child)
	{
		share = *(Share **)data.data;
		ft_share_ref (share);

#ifdef DEBUG_REMOVE
		if (order == 1)
		{
			GIFT_TRACEMEM (data.data, data.size);
			order = NULL;
		}
#endif

		/* FIXME */
		if (order)
			*order = NULL;
	}
	else
	{
		assert (data.size <= sizeof (*datarec));
		datarec = data.data;

#ifdef DEBUG_REMOVE
		if (order == 1)
		{
			GIFT_TRACEMEM (data.data, data.size);
			order = NULL;
		}
#endif

		/*
		 * Create a new file structure for this record...this is actually quite a
		 * bit less efficient than we want to be, but ft_search_tokenizef needs
		 * to be called to gaurantee we are gathering the exact token stream that
		 * was created at insert.
		 */
		if (!(share = unserialize_record (sdb, NULL, datarec, order)))
			return NULL;
	}
	return share;
}

/*****************************************************************************/

/*
 * This function is very poorly named.  What it actually means is lookup the
 * necessary information given an IP address and MD5 sum to construct a
 * completely unserialized FileShare structure and then add it to the search
 * results list.  This is utilized by both of the search interfaces.
 */
static BOOL add_search_result (Array **a, FTSearchDB *sdb, uint32_t id)
{
	Share *share;

	/**
	 * Search database handles will have their node identifier nullified when
	 * they are marked for removal (which often happens in the background
	 * while normal database operations are going on).  If db_search_md5 and
	 * db-search_tokens are working correctly, they will intelligently drop
	 * results scheduled for removal before they identify a match, so we
	 * shouldn't get here unless something went wrong.
	 */
	assert (sdb->node != NULL);
	assert (sdb->node->session != NULL);

	/*
	 * Retrieve the actual share record containing enough data to
	 * completely unserialize the original FileShare object inserted into
	 * the database.
	 */
	if (!(share = db_get_share (sdb, id, NULL)))
	{
		FT->DBGFN (FT, "%s: unable to lookup id %d",
		           ft_node_fmt (sdb->node), id);
		return FALSE;
	}

	/* last but not least, add the completely constructed file */
	return BOOL_EXPR (array_push (a, share));
}

static int db_search_md5 (Array **a, unsigned char *md5, int max_results)
{
	static struct md5idx_data *datarec;
	DB       *dbp;
	DBC      *dbcp;
	DBT       key;
	DBT       data;
	u_int32_t flags;
	int       results = 0;

	if (!(dbp = db_md5idx()))
		return 0;

	/*
	 * Create and position the cursor at the set resulting from the md5
	 * lookup in the master primary database.  In other words, a list of
	 * share host ip addresses that successfully matched this search.
	 */
	if (!(dbcp = cursor_md5idx_md5 (dbp, md5)))
		return 0;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	/* iterate the cursor */
	for (flags = DB_CURRENT;
	     dbcp->c_get (dbcp, &key, &data, flags) == 0;
	     flags = DB_NEXT_DUP)
	{
		assert (data.size == sizeof (*datarec));
		datarec = data.data;

		/* ignore results from nodes currently being removed in the
		 * background */
		if (datarec->sdb->node == NULL)
			continue;

		/*
		 * Add the search result.  This should NOT fail, unless something
		 * very bad is going on, as we have already confirmed through this
		 * loop that the data we are looking for is inserted in at least the
		 * master database.  This database design does not utilize stale
		 * entries.
		 */
		if (add_search_result (a, datarec->sdb, datarec->id))
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

	/* the token this is searching for */
	uint32_t  token;
	
	/* where we should store the order after lookup */ 
	uint8_t   *optr;

	/* whether this is an exclude token */
	BOOL       exclude;

	/* Minimum number of occurrences */
	int        count;

	/* duplicate count */
	db_recno_t  len;
};

static int cleanup_cursors (struct cursor_stream *s, void *udata)
{
	(s->cursor)->c_close (s->cursor);
	free (s);
	return TRUE;
}

static void token_cleanup (List *cursors)
{
	assert (!cursors || cursors->prev == NULL);
	list_foreach_remove (cursors, (ListForeachFunc)cleanup_cursors, NULL);
}

static DBC *get_cursor (DB *dbp, uint32_t token)
{
	static struct tokenidx_key keyrec;
	DBC *dbcp;
	DBT  key;
	DBT  data;
	int  ret;

	if (dbp->cursor (dbp, NULL, &dbcp, 0))
		return NULL;

	keyrec.token = token;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	key.data = &keyrec;
	key.size = sizeof (keyrec);

	/* TODO: how do I set the cursor without actually requiring a lookup of
	 * this data? */
	if ((ret = dbcp->c_get (dbcp, &key, &data, DB_SET)) != 0)
	{
		dbcp->c_close (dbcp);
		return NULL;
	}

	return dbcp;
}

static List *token_gather_cursors (DB *dbp, struct tokenized *query,
				   struct tokenized *exclude, uint8_t *ordmap)
{
	List     *cursors = NULL;
	DBC      *dbcp;
	uint8_t  *ordptr;
	int       i;

	/* ignore queries with just exclude tokens */
	if (!query->len)
		return NULL;

	for (i = 0, ordptr = ordmap; i < query->len; i++, ordptr++)
	{
		struct cursor_stream *s;

		/* one of the tokens searched for failed to lookup completely,
		 * abort the search (and return 0 results) */
		if (!(dbcp = get_cursor (dbp, query->tokens[i])))
		{
			token_cleanup (cursors);
			return NULL;
		}

		if (!(s = malloc (sizeof (struct cursor_stream))))
			continue;

		s->cursor = dbcp;
		s->flags  = DB_CURRENT;
		s->token   = query->tokens[i];
		s->optr    = ordmap ? ordptr : NULL;
		s->exclude = FALSE;
		s->count   = query->count[i];

		cursors = list_prepend (cursors, s);
	}

	for (i = 0; i < exclude->len; i++)
	{
		struct cursor_stream *s;

		/* skip any missing tokens: we didn't want them
		 * anyway */
		if (!(dbcp = get_cursor (dbp, exclude->tokens[i])))
			continue;

		if (!(s = malloc (sizeof (struct cursor_stream))))
			continue;

		s->cursor  = dbcp;
		s->flags   = DB_CURRENT;
		s->token   = exclude->tokens[i];
		s->optr    = NULL;
		s->exclude = TRUE;
		s->count   = 0;

		cursors = list_prepend (cursors, s);
	}

	return cursors;
}

/*
 * Search database handles being removed will nullify the node pointer to
 * indicate that they should not be considered for search results or other
 * lookups.  These handles are considered "stale", and will be checked for any
 * token match is given.
 */
static BOOL is_stale_db (DBT *data)
{
	struct tokenidx_data *datarec;
	FTSearchDB *sdb;

	assert (data->size == sizeof (*datarec));
	datarec = data->data;

	if (datarec->db_id == LOCAL_CHILD)
		return FALSE;

	sdb = child_lookup (datarec->db_id);

#ifndef SEARCH_DB_BENCHMARK
	if (sdb->node == NULL)
		assert (remove_active == TRUE);
#else
	assert (sdb->node != NULL);
#endif

	/* if node is NULL, stale is TRUE */
	return BOOL_EXPR (sdb->node == NULL);
}

static BOOL look_for (struct cursor_stream *s, DBT *data_cmp)
{
	static struct tokenidx_data *datarec_cmp;
	static struct tokenidx_data *datarec;
	DBT key;
	DBT data;
	int cmp;

	assert (data_cmp->size == sizeof (struct tokenidx_data));
	datarec_cmp = data_cmp->data;

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	/*
	 * Attempt to set this cursor to a similar position as the start cursor,
	 * while attempting to locate any possible token intersection according
	 * to the md5sum (compare cmp_data vs data).
	 */
	for (; s->flags && (s->cursor)->c_get (s->cursor, &key, &data, s->flags) == 0;
	     s->flags = DB_NEXT_DUP)
	{
		/* ignore nodes currently being removed */
		if (is_stale_db (&data) == TRUE)
			continue;

		assert (data.size == sizeof (*datarec));
		datarec = data.data;

		cmp = datarec->id - datarec_cmp->id;

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
		{
			struct tokenidx_data *rec = data.data;
					      
			/* check the count first */
			if (rec->count < s->count)
				continue;

			/* fill in the order while we have the
			 * tokenidx_data record available */
			if (s->optr)
				(*s->optr) = rec->order;

			return TRUE;
		}
	}

	s->flags = 0;

	/* this set has exhausted, no more data left */
	return FALSE;
}

static void calc_length (struct cursor_stream *s, void *udata)
{
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

	s->len = scnt;
}

static int compare_length (struct cursor_stream *a, struct cursor_stream *b)
{
	if (a->len > b->len)
		return 1;

	if (a->len < b->len)
		return -1;

	return 0;
}

static struct cursor_stream *get_start_cursor (List **qt)
{
	struct cursor_stream *s;
	List      *link;

	/*
	 * Loop through all cursor streams in order to calculate the shortest
	 * cursor (in terms of number of duplicates).  See below (match_tokens) for an
	 * explanation of why we do this.  Note that if we only have one
	 * element in this list we can assume it is the shortest and skip the
	 * cursor count retrieval.
	 */
	if (list_next (*qt))
	{
		List *ptr;
		uint32_t last_token = 0;
		
		list_foreach (*qt, (ListForeachFunc)calc_length, NULL);

		/*
		 * We sort separately rather than doing sorted inserts
		 * so that we can avoid counting if some of the tokens
		 * don't exist.
		 */
		*qt = list_sort (*qt, (CompareFunc)compare_length);

		/* 
		 * Verify that there are no duplicate tokens (which
		 * will now be adjacent). Duplicates can occur only
		 * when the same token appears in both the query and
		 * exclude lists, as each list was uniq'd
		 * individually.
		 */
		for (ptr = *qt; ptr; ptr = list_next (ptr))
		{
			s = ptr->data;
			
			if (s->token == last_token)
				return NULL;
			
			last_token = s->token;
		}

		/* loop until we find a query token */
		for (ptr = *qt; ptr; ptr = list_next (ptr))
		{
			s = ptr->data;

			if (s->exclude == FALSE)
				break;
		}

		link = ptr;
	}
	else
		link = *qt;

	/* remove from the cursor list and return the beginning cursor
	 * stream */

	if (!link)
		return NULL;

	/* we need to assign this before we remove the link as it will be
	 * freed by removal */
	s = link->data;
	*qt = list_remove_link (*qt, link);

	return s;
}

/*
 * We keep a list of the ordering of tokens in the query. This is
 * mapped to the actual values of the "order" fields in tokenidx_data
 * for each token, which give the token numbers used when this result
 * was added (and thus those used in the order list in
 * sharedata_data). Simple string comparison is then used to check
 * that each search result contains the mapped ordering(s) in the
 * share data. The order list for each share is a temporary copy, so,
 * after matching, we overwrite the matched orders with separators so
 * they can only match once.
 */
static BOOL check_order (uint8_t *share_order, uint8_t *order, uint8_t *ordmap)
{
	uint8_t *newmap, *ptr, *sptr;
	int ordlen, i;
	BOOL ret = TRUE;

	if (order == NULL ||
	    share_order == NULL)
		return TRUE;

	ordlen = strlen (order);
	
	newmap = MALLOC (ordlen + 1);
	
	for (i = 0, ptr = newmap; i <= ordlen; i++)
	{
		if (order[i] > ORDER_SEP)
			*(ptr++) = ordmap[order[i] - ORDER_MIN];
		else
		{
			/* we have a full phrase to check */
			*ptr = 0;
			sptr = strstr (share_order, newmap);

			if (sptr == NULL)
			{			
				ret = FALSE;
				break;
			}
			
			/* erase it so it won't match next time */
			memset (sptr, ORDER_SEP, ptr-newmap);

			ptr = newmap;
		}
	}

	free (newmap);

	return ret;
}

static int match_tokens (Array **results, List **qt, int max_results,
		     uint8_t *order, uint8_t *ordmap)
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
		struct tokenidx_data *rec = data.data;

		/* quick check to make sure this item does't belong to a node
		 * currently being removed */
		if (is_stale_db (&data) == TRUE)
			continue;

		lost = FALSE;

		/* check the count first */
		if (rec->count < s->count)
			continue;

		/* fill in the order while we have the
		 * tokenidx_data record available */
		if (s->optr)
			(*s->optr) = rec->order;

		/*
		 * Walk along all the other tokens looking for an intersection.  Note
		 * that this code holds the last position of the cursor so that we
		 * never have to begin the search from the beginning.
		 */
		for (ptr = *qt; ptr; ptr = list_next (ptr))
		{
			if (look_for (ptr->data, &data) ==
			    ((struct cursor_stream *)(ptr->data))->exclude)
			{
				lost = TRUE;
				break;
			}
		}

		/*
		 * This token matched in all cursors.  It should be considered a
		 * positive hit.
		 */
		if (lost == FALSE)
		{
			struct tokenidx_data *datarec = data.data;
			uint8_t *share_order;
			Share *share;
			FTSearchDB *sdb = child_lookup (datarec->db_id);

			/* grab the Share and order list */
			if (!(share = db_get_share (sdb, datarec->id, &share_order)))
			{
				FT->DBGFN (FT, "%s: unable to lookup id %d",
					   ft_node_fmt (sdb->node),
					   datarec->id);
				db_abort (sdb);

				continue;
			}

			/* do some more verification first */
			if (order && check_order (share_order, order, ordmap) == FALSE)
			{
				ft_share_unref (share);
				free (share_order);

				continue;
			}
			
			free (share_order);

			/* and add it */
			if (max_results)
				array_push (results, share);

			/* make sure we cap the size of the results */
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
                             struct tokenized *query, struct tokenized *exclude,
			     int max_results)
{
	DB   *dbp;
	List *cursors = NULL;
	int   results = 0;
	uint8_t *ordmap = NULL;

	if (!(dbp = db_tokenidx()))
		return 0;

	/* map the tokens list to their ordering in the original
	 * record */
	if (query->order)
		ordmap = CALLOC (query->len, 1);

	/* construct a list of all positioned cursors, effectively retrieving a
	 * list of token result streams */
	cursors = token_gather_cursors (dbp, query, exclude, ordmap);

	results = match_tokens (a, &cursors, max_results,
				      query->order, ordmap);

	token_cleanup (cursors);
	
	free (ordmap);

	return results;
}

/*****************************************************************************/

#endif /* USE_LIBDB */

/*****************************************************************************/

BOOL ft_search_db_init (const char *envpath, unsigned long cachesize)
{
	if (db_initialized)
		return db_initialized;

#ifdef USE_LIBDB
	/* save the initialized path for destroy */
	if (!(env_search_path = gift_strdup (envpath)))
		return FALSE;

	/* delete any host shares left over from previous search node sessions */
	clean_db_path (env_search_path);

	if (!(db_initialized = db_init (env_search_path, (u_int32_t)cachesize)))
	{
		free (env_search_path);
		env_search_path = NULL;
	}

	init_children ();

	local_child = search_db_new (NULL);
#ifdef BLOOM
	local_child->filter = md5_filter.filter;
#endif

#endif /* USE_LIBDB */

	return db_initialized;
}

void ft_search_db_destroy (void)
{
	if (!db_initialized)
		return;

#ifdef USE_LIBDB
	/* Clean up all the individual dbs.  Even though the db
	 * environment and all it contains is about to be destroyed,
	 * we need to delete everything to make sure it isn't synced
	 * to disk (and to clean up all the non-db structures that
	 * would otherwise leak).
	 */
	{
		int i;
		
		/* local_child is included in this list */
		for (i=0; i < MAX_CHILDREN; i++)
		{
			FTSearchDB *sdb = child_index[i];

			if (sdb)
			{
				db_close (sdb, TRUE);
				search_db_free (sdb);
			}
		}

		/* FIXME: de-ickify this mess */
		close_db (db_md5_idx, "md5.index", NULL, TRUE);
		close_db (db_token_idx, "tokens.index", NULL, TRUE);
		close_db (db_share_data, "share.data", NULL, TRUE);
	}

	assert (env_search_path);
	{
		db_destroy (env_search_path);

		free (env_search_path);
		env_search_path = NULL;
	}

#endif /* USE_LIBDB */

	db_initialized = FALSE;
}

/*****************************************************************************/

BOOL ft_search_db_insert (FTNode *node, Share *share)
{
	BOOL ret = FALSE;

	if (!node || !share)
		return FALSE;

#ifdef USE_LIBDB
	assert (node->session != NULL);

	if (!FT_SEARCH_DB(node))
	{
		FT->DBGFN (FT, "insertion requested without a child object!");
		return FALSE;
	}

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
	uint32_t id;
	off_t size = 0;
#endif /* USE_LIBDB */

	if (!node || !md5)
		return FALSE;

#ifdef USE_LIBDB
	/*
	 * Grab the per-user data entry at the supplied key, which will contain
	 * enough information to get the token list for removal from the
	 * secondary database and the size for statistics purposes.
	 */
	if (!(id = db_lookup_md5 (FT_SEARCH_DB(node), md5)))
	{
		FT->DBGFN (FT, "%s: unable to locate md5 %s for removal",
		           ft_node_fmt (node), md5_fmt (md5));
		return FALSE;
	}

	if ((ret = db_remove (FT_SEARCH_DB(node), id, &size)))
	{
		FT_SEARCH_DB(node)->shares--;
		FT_SEARCH_DB(node)->size -= ((float)size / 1024.0) / 1024.0;
#ifndef SEARCH_DB_BENCHMARK
		FT->DBGFN (FT, "%s: removed '%s' (%d, %d left)", ft_node_fmt(node), md5_fmt(md5), size, FT_SEARCH_DB(node)->shares);
#endif
	}
	else
	{
		FT->DBGFN (FT, "%s: '%s' removal failed", ft_node_fmt(node), md5_fmt(md5));
	}
#endif /* USE_LIBDB */

	return ret;
}

BOOL ft_search_db_insert_local (Share *share)
{
	BOOL ret = FALSE;

	if (!share)
		return FALSE;

#ifdef USE_LIBDB
	ret = db_insert (local_child, share);
#endif /* USE_LIBDB */

	return ret;
}

BOOL ft_search_db_remove_local (Share *share)
{
	BOOL ret    = FALSE;
	uint32_t id;

	if (!share)
		return FALSE;

#ifdef USE_LIBDB
	id = db_lookup_local_share (share);

	if (id && (ret = db_remove (local_child, id, NULL)))
	{
#ifndef SEARCH_DB_BENCHMARK
		FT->DBGFN (FT, "local: removed %s", share->path);
#endif
	}
	else
	{
		assert (id == 0);
		FT->DBGFN (FT, "local: %s removal failed", share->path);
	}
#endif /* USE_LIBDB */

	return ret;
}

BOOL ft_search_db_remove_host (FTNode *node)
{
#ifdef USE_LIBDB
	FTSearchDB *sdb;
#endif /* USE_LIBDB */
	int ret = TRUE;

	if (!node)
		return FALSE;

#ifdef USE_LIBDB
	sdb = node->session->search_db;    /* FT_SEARCH_DB(node) */
	node->session->search_db = NULL;

	if (!sdb)
		return TRUE;

#ifndef SEARCH_DB_BENCHMARK
	if (openft->shutdown)
		return TRUE;
#endif

	/*
	 * Schedule the removal and leave.  We cannot block for the entire
	 * duration of this operation, but we can disassociate the FTSearchDB
	 * from any active results and begin removing in the background.  Only
	 * one user database is processed at any given time.
	 */
	ret = db_remove_host_schedule (sdb);
#endif /* USE_LIBDB */

	return ret;
}

BOOL ft_search_db_sync (FTNode *node)
{
	BOOL ret = TRUE;

#ifdef USE_LIBDB
# ifdef SEARCH_DB_SYNC
	ret = db_sync (FT_SEARCH_DB(node));
# endif /* SEARCH_DB_SYNC */
#endif /* USE_LIBDB */

	return ret;
}

BOOL ft_search_db_open (FTNode *node)
{
	FTSearchDB *sdb;

	if (!node)
		return FALSE;

#ifdef USE_LIBDB
	/* if this is our first time operating on this node, create a search
	 * database handle */
	if (!(sdb = FT_SEARCH_DB(node)))
	{
		/* the internal interface will access the node object circularly to
		 * improve isolation of this interface */
		if (!(sdb = FT_SEARCH_DB(node) = search_db_new (node)))
			return FALSE;
	}

	/* open the only database that is likely to be closed */
	if (!(db_shareidx (sdb, TRUE)))
		return FALSE;

#ifdef BLOOM
	if (!sdb->filter)
		sdb->filter = ft_bloom_new (MD5_FILTER_BITS, MD5_FILTER_HASHES,
					    16 << 3, FALSE);

	assert (sdb->filter);
	
	if (!sdb->old_filter)
		sdb->old_filter = ft_bloom_clone (sdb->filter);
#endif
#endif /* USE_LIBDB */

	return TRUE;
}

BOOL ft_search_db_isopen (FTNode *node)
{
	BOOL ret = FALSE;

#ifdef USE_LIBDB
	if (FT_SEARCH_DB(node) && FT_SEARCH_DB(node)->share_idx)
		ret = TRUE;
#endif /* USE_LIBDB */

	return ret;
}

BOOL ft_search_db_close (FTNode *node, BOOL rm)
{
	FTSearchDB *sdb;

	if (!node)
		return FALSE;

#ifdef USE_LIBDB
	sdb = FT_SEARCH_DB(node);

	if (!db_close (sdb, rm))
		return FALSE;

#ifdef BLOOM
	assert (sdb->filter);
	assert (sdb->old_filter);

	ft_bloom_unmerge (sdb->old_filter, md5_filter.filter);
	ft_bloom_merge (sdb->filter, md5_filter.filter);
	ft_bloom_free (sdb->old_filter);

	sdb->old_filter = NULL;
#endif
#endif /* USE_LIBDB */

	return TRUE;
}

/*****************************************************************************/

#if 0
Share *ft_search_db_lookup_md5 (FTNode *node, unsigned char *md5)
{
	Share *ret = NULL;

	if (!node || !md5)
		return NULL;

#ifdef USE_LIBDB
	ret = db_lookup_md5 (FT_SEARCH_DB(node), md5, NULL);
#endif /* USE_LIBDB */

	return ret;
}
#endif

int ft_search_db_md5 (Array **a, unsigned char *md5, int max_results)
{
	int results = 0;

	if (!md5 || max_results <= 0)
		return results;

#ifdef USE_LIBDB
	results = db_search_md5 (a, md5, max_results);

#ifdef BLOOM
	/* a little paranoia never hurt anybody */ 
	{
		static int total=0, hits=0, misses=0,
			misfiltered=0, unsynced=0;
		
		BOOL fret = ft_bloom_lookup (md5_filter.filter, md5);

		total++;

		if (results)
		{
			hits++;

			/* we got at least one hit, make sure we
			 * wouldn't have discarded it */
			if (!fret)
			{
#if 0
				/* FIXME */
				assert (ft_bloom_lookup (md5_filter.sync, md5));
#endif

				unsynced++;
			}
		}
		else
		{
			misses++;

			if (fret)
				misfiltered++;
		}
		
		if (total % 1000 == 0)
			FT->DBGFN (FT, "total=%d, hits=%d, misses=%d, misf=%d, unsynced=%d, phit=%f, fpos=%f, fneg=%f, density=%f", total, hits, misses, misfiltered, unsynced, ((double)hits)/total, ((double)misfiltered)/misses, ((double)unsynced)/hits, ft_bloom_density (md5_filter.filter));
	}
#endif
#endif /* USE_LIBDB */

	return results;
}

int ft_search_db_tokens (Array **a, char *realm,
                         struct tokenized *query, struct tokenized *exclude,
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

/*****************************************************************************/

/*
 * Submitted by Tom Hargreaves <hex@freezone.co.uk>, heavily modified style
 * by jasta.  This code is extremely ugly and is intended for
 * debugging/testing only.  To build the OpenFT module for use with this
 * test suite, use:
 *
 * make CFLAGS='-DOPENFT_TEST_SUITE=test_suite_search_db -g3 -O0 -Wall'
 *
 * Invoke with:
 *
 * giftd -v -p ./src/libOpenFT.la
 *
 * To use this code you will also need to produce a valid sync'd
 * children.data file from an actively running OpenFT search node session.  I
 * recommend enabling SEARCH_DB_SYNC and then just copying it once it becomes
 * sufficiently large.  Also, you will need a list of queries to perform for
 * testing.  Using the following script you can extract from your already
 * running daemon log file:
 *
 * cat ~/.giFT/giftd.log | \
 *   perl -lne 'm/exec_search.*'\''(.*?)'\''/ and print $1;' > queries
 *
 */
#ifdef OPENFT_TEST_SUITE

static struct hl
{
	FTNode *node;
	Array  *files;
} *nodelist;

static int nodes = 0;
static int files = 0;

static int minnodes = 100;
static int maxnodes = 500;

static int minqueries =  5000;
static int maxqueries = 10000;

static Array *queries = NULL;

static void bm_close_db (DB *dbp)
{
        if (!dbp)
                return;

        dbp->close (dbp, 0);
}

static DB *bm_open_db (const char *file, const char *database)
{
        DB       *dbp;
        DBTYPE    type  = DB_UNKNOWN;
        u_int32_t flags = DB_RDONLY;
        int       mode  = 0664;
        int       ret;

        if (db_create (&dbp, NULL, 0))
                return NULL;

#if DB_VERSION_MAJOR > 4 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 1)
        ret = dbp->open (dbp, NULL, file, database, type, flags, mode);
#else
        ret = dbp->open (dbp, file, database, type, flags, mode);
#endif

        if (ret)
        {
                bm_close_db (dbp);
                return NULL;
        }

        return dbp;
}

static int get_file_records (FTNode *node, DB *hostdb, Array **files)
{
	DBT  key, data;
	DBC *curs;
	int  cget_ret;
	int  nfiles = 0;
	int  flags = DB_FIRST;
	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	if (hostdb->cursor (hostdb, NULL, &curs, 0) != 0)
	{
		FT->err (FT, "cursor initialization failed");
		abort ();
	}

	while ((cget_ret = curs->c_get (curs, &key, &data, flags)) == 0)
	{
		Share         *record;
		unsigned char *md5 = key.data;

		record = unserialize_record (FT_SEARCH_DB(node), md5, data.data, NULL);
		assert (record != NULL);

		array_push (files, record);

#if 1
		/* make some duplicates for good measure */
		if (rand() < RAND_MAX/100)
		{
			ft_share_ref (record);
			array_push (files, record);
			nfiles++;
		}
#endif

		nfiles++;

		flags = DB_NEXT;
	}

	if (cget_ret != DB_NOTFOUND)
	{
		FT->err (FT, "DBcursor->c_get: %d", cget_ret);
		abort ();
	}

	return nfiles;
}

static int load_test_data (const char *dbfile)
{
	DB            *testdb;
	DB_BTREE_STAT *stats;
	DBT            key, data;
	DBC           *curs;
	int            cget_ret;
	int            i = 0;

	if (!(testdb = bm_open_db (dbfile, NULL)))
	{
		FT->err (FT, "failed to open db '%s'", dbfile);
		return -1;
	}

	if (testdb->stat (testdb, &stats, 0) != 0)
	{
		FT->err (FT, "%s: stats request failed", dbfile);
		return -1;
	}

	nodes = stats->bt_ndata;
	FT->dbg (FT, "%s: contains %d nodes", dbfile, nodes);

	if (nodes > maxnodes)
		nodes = maxnodes;

	assert (nodes >= minnodes);

	nodelist = calloc (nodes, sizeof (struct hl));
	assert (nodelist != NULL);

	if (testdb->cursor (testdb, NULL, &curs, 0))
	{
		FT->err (FT, "%s: cursor initialization failed", dbfile);
		return -1;
	}

	memset (&key, 0, sizeof (key));
	memset (&data, 0, sizeof (data));

	while ((cget_ret = curs->c_get (curs, &key, &data, DB_NEXT)) == 0)
	{
		DB         *hostdb;
		FTNode     *node;
		FTSearchDB *sdb;
		char        dbname[256];
		char        hname[256], *hname_ptr;
		struct hl   hl;
		int         hfiles = 0;

		assert (key.size <= sizeof (hname));
		gift_strncpy (hname, key.data, key.size);

		if ((hname_ptr = strchr (hname, ':')))
			*hname_ptr = 0;

		node = ft_node_new (net_ip (hname));
		assert (node != NULL);

		assert (node->session == NULL);
		node->session = MALLOC (sizeof (FTSession));

		sdb = search_db_new (node);
		assert (sdb != NULL);

		assert (key.size <= sizeof (dbname));
		gift_strncpy (dbname, key.data, key.size);

		if (!(hostdb = bm_open_db (dbfile, dbname)))
		{
			FT->err (FT, "%s: failed to open '%s'", dbfile, dbname);
			continue;
		}

		hl.node = node;

		if (hostdb->stat (hostdb, &stats, 0) != 0)
		{
			FT->err (FT, "%s(%s): failed to get stats", dbfile, hname);
			abort ();
		}

		hfiles = stats->bt_ndata;
		files += hfiles;

		hl.files = array_new (NULL);
		assert (hl.files != NULL);

		get_file_records (node, hostdb, &hl.files);
#if 0
		FT->dbg (FT, "%s(%s): loaded %d files", dbfile, hname,
		         array_count (&hl.files));
#endif

		nodelist[i++] = hl;

		bm_close_db (hostdb);

		if (i >= maxnodes)
			break;
	}

	nodes = i;

	FT->dbg (FT, "loaded %d total files (average %d)", files, files / nodes);

	if (cget_ret != 0 && cget_ret != DB_NOTFOUND)
	{
		FT->err (FT, "%s: DBcursor->c_get: %d", dbfile, cget_ret);
		abort ();
	}

	bm_close_db (testdb);

	return files;
}

static int load_queries (const char *filename)
{
	FILE *f;
	char  buf[256];
	int   n = 0;

	queries = array_new (NULL);
	assert (queries != NULL);

	if (!(f = fopen (filename, "r")))
	{
		FT->err (FT, "error reading queries from '%s'", filename);
		return -1;
	}

	while (fgets (buf, sizeof (buf), f) && ++n < maxqueries)
	{
		buf[strlen(buf)-1] = 0;        /* chop */

		if (!(array_push (&queries, strdup (buf))))
		{
			FT->err (FT, "error adding search query '%s'", buf);
			continue;
		}
	}

	fclose (f);

	/* assert (n >= minqueries); */

	FT->dbg (FT, "loaded %d search queries", n);

	return n;
}

static void free_queries (void)
{
	char *query;

	while ((query = array_shift (&queries)))
	{
		free (query);
	}

	array_unset (&queries);
}

static void free_test_data (void)
{
	int i;
	for (i = 0; i < nodes; i++)
	{
		FTNode *node = nodelist[i].node;
		Array *files = nodelist[i].files;
		Share *record;

		while ((record = array_shift (&files)))
			ft_share_unref (record);

		array_unset (&files);

		ft_node_free (node);
	}
	 
	free (nodelist);
}

static double run_insert (void)
{
	StopWatch *gsw;
	double     itime;
	int        i;

	gsw = stopwatch_new (TRUE);
	assert (gsw != NULL);

	for (i = 0; i < nodes; i++)
	{
		struct hl *hl = &nodelist[i];
		int flen = array_count (&hl->files);
		int j;

		ft_search_db_open (hl->node);

		for (j = 0; j < flen; j++)
		{
			Share *share = array_index (&hl->files, j);
			Hash *hash=share_get_hash (share, "MD5");
			assert (share != NULL);

#if 0
			assert (hash);
			memset (hash->data, 0, 16);
#endif

			if (!(ft_search_db_insert (hl->node, share)))
			{
				FT->err (FT, "%s(%s): error inserting file",
				         share->path, ft_node_fmt (hl->node));
				abort ();
			}
		}

		ft_search_db_close (hl->node, FALSE);
		ft_search_db_sync (hl->node);
	}

	itime = stopwatch_free_elapsed (gsw);

	return itime;
}

/* token or hash searches */
#define TOKENS
static double run_search (void)
{
	StopWatch *gsw;
	double     stime;
	int        i;
	int        nqueries = array_count (&queries);

	gsw = stopwatch_new (TRUE);
	assert (gsw != NULL);

#ifndef INTERACTIVE
	for (i = 0; i < nqueries; i++)
	{
		char *query = array_index (&queries, i);
		char *exclude = "";
#else
	char query[100];
	char exclude[100];

	db_sync (NULL);

	/* fixed size buffers *and* gets()?! 
	 * hey, this is only for testing... */
	while (gets (query))
	{
		StopWatch *sw;

#endif
		struct tokenized *qtok, *etok;
		Array *matches = NULL;
		int hits;
		int j;
		unsigned char *md5;

#ifdef INTERACTIVE
#ifdef TOKENS
		gets (exclude);
#else
		exclude[0] = '\0';
#endif
		sw = stopwatch_new (TRUE);
#endif

#ifdef TOKENS
		qtok = ft_tokenize_query (query, TOKENIZE_ORDER | TOKENIZE_COUNT);
		assert (qtok != NULL);
		etok = ft_tokenize_query (exclude, 0);
		assert (etok != NULL);

		hits = ft_search_db_tokens (&matches, NULL, qtok, etok, 100000);

		ft_tokenize_free (qtok);
		ft_tokenize_free (etok);
#else
		md5 = md5_bin (query);
		hits = ft_search_db_md5 (&matches, md5, 100000);
#endif

#ifdef INTERACTIVE
		printf("'%s' (-'%s'): %d hits, %.06f elapsed\n", query, exclude, hits, stopwatch_free_elapsed (sw));
#endif

		for (j = 0; j < hits; j++)
		{
			Share *share = array_index (&matches, j);
#ifdef INTERACTIVE
			if (hits < 30)
				printf ("%s\n", share->path);
#endif
			ft_share_unref (share);
		}

		array_unset (&matches);
	}

	stime = stopwatch_free_elapsed (gsw);

	return stime;
}

static double run_remove (BOOL remove_singly)
{
	StopWatch *gsw;
	double rtime;
	int i;

	gsw = stopwatch_new (TRUE);
	assert (gsw != NULL);

	for (i = 0; i < nodes; i++)
	{
		FTNode *node = nodelist[i].node;

#if 1
		StopWatch *sw = stopwatch_new (TRUE);
		int shares = FT_SEARCH_DB(node)->shares;
		assert (sw != NULL);
#endif

		if (remove_singly)
		{
			int j;
			Array *f=nodelist[i].files;
			int flen=array_count(&f);

			ft_search_db_open (node);

			for(j=0;j<flen /*/2*/;j++) {
				Share *file=array_splice (&f, j, 0, NULL);
				Hash *hash=share_get_hash (file, "MD5");
				if (hash) {
#if 0
					FT->dbg (FT,"removing file %s (%s)", file->path, ft_node_fmt(node));
#endif

					if (!ft_search_db_remove(node,hash->data))
					{
						FT->err (FT,"error removing file %s (%s)", file->path, ft_node_fmt(node));
						abort();
					}
				} else 
					FT->err (FT, "error reading file array");
			}
			
			ft_search_db_close (node, FALSE);

#if 0
			check_tokens (FT_SEARCH_DB(node));
#endif

#if 0
			run_search ();
#endif

			{
				DB_BTREE_STAT *stats;
				DB *db=FT_SEARCH_DB(node)->share_idx;
				if (db)
				{
					if (!db->stat(db, &stats, 0))
						assert (stats->bt_ndata == 0);
					else
					{
						FT->err(FT, "failed to get sdb stats");
						abort ();
					}
				}
			}
		}

		if (0)
		{
			if (!(ft_search_db_remove_host (node)))
			{
				FT->err (FT, "error removing node %s", ft_node_fmt (node));
				abort ();
			}
		}

#if 1
		FT->dbg (FT, "delete %s(%lu): %.06fs elapsed", ft_node_fmt (node),
				 shares, stopwatch_free_elapsed (sw));
#endif

		ft_search_db_sync (node);
	}

	rtime = stopwatch_free_elapsed (gsw);

	return rtime;
}

static void test_benchmarks (void)
{
	double itime, rtime, stime;
	int nqueries = array_count (&queries);

	/* insert */
	itime = run_insert ();
	FT->dbg (FT, "insert(%lu): %.06fs elapsed (avg %.02f files/s)",
	         files, itime, (float)files / itime);

	/* search */
	stime = run_search ();
	FT->dbg (FT, "search(%lu): %.06fs elapsed (avg %.02f queries/s)",
	         nqueries, stime, (float)nqueries / stime);

	/* remove */
	rtime = run_remove (TRUE);
	FT->dbg (FT, "remove(%lu): %.06fs elapsed (avg %.02f files/s)",
	         files, rtime, (float)files / rtime);

	db_sync (NULL);
}

BOOL test_suite_search_db (Protocol *p)
{
#ifndef SEARCH_DB_BENCHMARK
	FT->err (FT, "benchmarking requested but disabled");
	return FALSE;

#else
	int ret;

	/* FIXME: should be later */
	ft_search_db_init ("benchtemp", 50<<20);

	if ((ret = load_test_data ("test.data")) <= 0)
	{
		FT->err (FT, "test.data: no test data found");
		return FALSE;
	}

	if ((ret = load_queries ("queries")) <= 0)
	{
		FT->err (FT, "queries: no queries found");
		return FALSE;
	}

	test_benchmarks ();

	free_test_data ();
	free_queries ();

	return TRUE;
#endif
}

#endif /* OPENFT_TEST_SUITE */
