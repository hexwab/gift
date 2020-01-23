#include "ft_openft.h"
#include "meta.h"
#include "protocol.h"
#include "plugin.h"
#include "ft_conf.h"
#include "ft_shost.h"
#include "md5.h"
#include "array.h"

#include "ft_search_db.c"

Protocol *FT=NULL;
  
static void bm_close_db (DB *dbp)
{
        if (!dbp)
                return;

        dbp->close (dbp, 0);
}

static DB *bm_open_db (char *file, char *database)
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

#define LOGMSG(fmt,pfx)                                                  \
        char    msg[4096];                                                   \
        size_t  msgwr = 0;                                                   \
        va_list args;                                                        \
        if (pfx)                                                             \
                msgwr += snprintf (msg, sizeof (msg) - 1, "%s: ", (char *)pfx);  \
        va_start (args, fmt);                                                \
        vsnprintf (msg + msgwr, sizeof (msg) - msgwr - 1, fmt, args);        \
        va_end (args);


static int dbg_wrapper (Protocol *p, char *fmt, ...)
{
        LOGMSG (fmt, p->name);
        log_print (LOG_DEBUG, msg);

        return TRUE;
}

static int trace_wrapper (Protocol *p, char *file, int line, char *func,
                                                  char *fmt, ...)
{
        LOGMSG (fmt, NULL);

        return dbg_wrapper (p, "%s:%i(%s): %s", file, line, func, msg);
}

static int warn_wrapper (Protocol *p, char *fmt, ...)
{
        LOGMSG (fmt, p->name);
        log_warn ("%s", msg);

        return TRUE;
}

static int err_wrapper (Protocol *p, char *fmt, ...)
{
        LOGMSG (fmt, p->name);
        log_error ("%s", msg);

        return TRUE;
}

/*****************************************************************************/

#define TOKEN_PUNCT ",`'!?*"
#define TOKEN_DELIM "\\/ _-.[]()\t"

/*****************************************************************************/

static void token_remove_punct (char *token)
{
	char *ptr;

	if (!token[0])
		return;

	while ((ptr = string_sep_set (&token, TOKEN_PUNCT)))
	{
		if (!token)
			continue;

		string_move (token - 1, token);
		token--;
	}
}

static uint32_t tokenize_str (char *str)
{
	uint32_t hash = 0;

	if (!str)
		return 0;

	if (!(hash = *str++))
		return 0;

	while (*str)
		hash = (hash << 5) - hash + *str++;

	return hash;
}

static int cmp_token (const void *a, const void *b)
{
	uint32_t *ta = (uint32_t *)a;
	uint32_t *tb = (uint32_t *)b;

	if (*ta < *tb)
		return -1;

	if (*ta > *tb)
		return 1;

	return 0;
}

static int remove_dups (uint32_t *tokens, int num)
{
	uint32_t lt = 0;
	int      nt;
	int      t;

	if (num <= 0)
		return num;

	/* sort first */
	qsort (tokens, num, sizeof (uint32_t), cmp_token);

	for (t = 0, nt = 0; t < num && nt < num; t++)
	{
		if (lt && tokens[t] == lt)
			continue;

		lt = tokens[t];
		tokens[nt++] = tokens[t];
	}

	tokens[nt] = 0;

	return nt;
}

static int tokens_resize (uint32_t **tokens, int *alloc, int newsize)
{
	uint32_t *resize;

	if (!(resize = realloc (*tokens, newsize * sizeof (uint32_t))))
		return FALSE;

	*tokens = resize;
	*alloc = newsize;

	return TRUE;
}

/*
 * tokens are delimited by the set TOKEN_DELIM
 * tokens are stripped by the set TOKEN_PUNCT
 */
static void token_add_str (uint32_t **tokens, int *tok, int *tokalloc,
                           char *s)
{
	char *sdup, *s0;
	char *tstr;

	if (!*tokalloc)
	{
		assert (*tokens == NULL);
		if (!tokens_resize (tokens, tokalloc, 32))
			return;
	}

	if (!(sdup = STRDUP (s)))
		return;

	s0 = sdup;

	while ((tstr = string_sep_set (&sdup, TOKEN_DELIM)))
	{
		if (*tok >= (*tokalloc - 1))
		{
			if (!tokens_resize (tokens, tokalloc, *tokalloc * 2))
				break;
		}

		token_remove_punct (tstr);

		if (*tstr)
			(*tokens)[(*tok)++] = tokenize_str (string_lower (tstr));
	}

	free (s0);

	(*tokens)[*tok] = 0;
	*tok = remove_dups (*tokens, *tok);
}

uint32_t *ft_search_tokenize (char *string)
{
	uint32_t *tokens   = NULL;
	int       tok      = 0;
	int       tokalloc = 0;

	if (!string)
		return NULL;

	token_add_str (&tokens, &tok, &tokalloc, string);

	return tokens;
}

uint32_t *ft_search_tokenizef (FileShare *file)
{
	uint32_t *tokens   = NULL;
	int       tok      = 0;
	int       tokalloc = 0;

	if (!file)
		return NULL;

	token_add_str (&tokens, &tok, &tokalloc, file->path);
	token_add_str (&tokens, &tok, &tokalloc, share_get_meta (file, "tracknumber"));
	token_add_str (&tokens, &tok, &tokalloc, share_get_meta (file, "artist"));
	token_add_str (&tokens, &tok, &tokalloc, share_get_meta (file, "album"));
	token_add_str (&tokens, &tok, &tokalloc, share_get_meta (file, "title"));
	token_add_str (&tokens, &tok, &tokalloc, share_get_meta (file, "genre"));

	return tokens;
}

extern Dataset *mime_types;

struct hl {
	FTSHost *host;
	Array *files;
} *hostlist;
int hosts=0, files=0;
int maxhosts=100;
Array *queries;

int load_test_data(char *db_name) {
	DB *test_db;

	if (!(test_db = bm_open_db(db_name, NULL)))
	{
		FT->err(FT, "failed to open db '%s'",db_name);
		return 1;
	}

	{
		DB_BTREE_STAT *stats;
		DBT key, value;
		DBC *cur;
		int ret;
		int i=0;
		if (!test_db->stat(test_db, &stats, 0))
			FT->dbg(FT, "test db contains %d hosts", hosts=stats->bt_ndata);
		else
		{
			FT->err(FT, "failed to get test db stats");
			return 1;
		}
		if (maxhosts>hosts)
			maxhosts=hosts;
		hostlist=malloc(hosts*sizeof(struct hl));

		if (test_db->cursor (test_db, NULL, &cur, 0))
			FT->err(FT,"cursor init failed");
		
		memset (&key, 0, sizeof (key));
		memset (&value, 0, sizeof (value));

		while ((ret = cur->c_get (cur, &key, &value, DB_NEXT)) == 0) {
			DB *db;
			FTSHost *host;
			char hname[256];
			struct hl hl;
			int hfiles=0;

			memcpy(hname, key.data, key.size);
			hname[key.size]=0;
			
//			FT->dbg(FT, "adding host %s", hname);
			host=ft_shost_new(0, net_ip(hname), 0, 0, hname);
			db=bm_open_db(db_name, hname);
			if (!db)
				FT->err(FT, "failed to open db for host %s", hname);
			if (!host)
				FT->err(FT, "failed to create shost for %s",hname);
			hl.host=host;
			if (!db->stat(db, &stats, 0))
				hfiles=stats->bt_ndata;
			else
				FT->err(FT, "failed to get stats for %s", hname);
			hl.files=array(NULL);
//			FT->dbg(FT, "%s sharing %d files", hname, hfiles);
			files+=hfiles;

			{
				DBT fkey, fvalue;
				DBC *fcur;
				int fret;
				memset (&fkey, 0, sizeof (fkey));
				memset (&fvalue, 0, sizeof (fvalue));

				if (db->cursor (db, NULL, &fcur, 0))
					FT->err(FT,"cursor init failed");

				while ((fret = fcur->c_get (fcur, &fkey, &fvalue, DB_NEXT)) == 0) {
					FileShare *file;
					unsigned char *md5=fkey.data;
					file=unserialize_record(host, md5, fvalue.data);
//					FT->dbg(FT, "filename=%s",file->path);
					if (!file)
						FT->DBGFN(FT, "unserialize_record failed");
					else
						push(&hl.files, file);
				}
				if (fret != DB_NOTFOUND)
					FT->err (FT, "DBcursor->c_get: %d", fret);
			}
			hostlist[i++]=hl;

			bm_close_db(db);
			if (i>maxhosts)
				break;
		}

		hosts=i-1;

		FT->dbg(FT, "loaded %d total files (average %d)", files, files/hosts);

		if (ret && ret != DB_NOTFOUND)
			FT->err (FT, "DBcursor->c_get: %d", ret);
	}

	bm_close_db(test_db);
	return 0;
}

static int load_queries(char *filename)
{
	FILE *f;
	char buf[100];

	queries=array(NULL);

	if (!(f=fopen(filename, "r")))
	{
		FT->err(FT, "error reading search queries");
		return 1;
	}

	while (fgets(buf, sizeof(buf), f))
	{
		buf[strlen(buf)-1]=0; /* chop */
		if (!push(&queries, strdup(buf)))
			FT->err(FT,"error adding search query");
	}

	fclose(f);
	FT->dbg(FT, "loaded %d search queries", count(&queries));
	return 0;
}

void dummy (FTSHost *f) 
{
}

int do_benchmarks() {
	int i,j;
	StopWatch *gsw;
	double itime,rtime,stime;
	int nqueries=count(&queries);

	/* insert */
	gsw = stopwatch_new (TRUE);
	for(i=0;i<maxhosts;i++) {
		StopWatch *sw;
		FTSHost *shost=hostlist[i].host;
		Array *files=hostlist[i].files;
		int flen=count(&files);
//		sw = stopwatch_new (TRUE);
//		FT->dbg(FT, "inserting %d shares from %s",flen,net_ip_str(shost->host));
		for(j=0;j<flen;j++) {
			FileShare *file=splice (&files, j, 0, NULL);
			if (file) {
				if (!ft_search_db_insert(shost,file))
					FT->err(FT,"error inserting file %s (%s)", file->path, net_ip_str(shost->host));
			} else 
				FT->err(FT, "error reading file array");
		}
//			FT->dbg (FT, "%s(%lu): %.06fs elapsed", net_ip_str (shost->host),
//				 shost->shares, stopwatch_free_elapsed (sw));
		ft_search_db_sync (shost);
	}

	//	exit(0);
	itime=stopwatch_free_elapsed (gsw);

	/* search */
	gsw = stopwatch_new (TRUE);
	for(i=0;i<nqueries;i++) {
		char *query=splice (&queries, i, 0, NULL);
		uint32_t *qtokens;
		uint32_t etokens=0;
		Array *matches=NULL;
		int hits;

		qtokens = ft_search_tokenize (query);
		if (!qtokens)
			FT->err(FT, "error tokenizing '%s'", query);
		hits = ft_search_db_tokens (&matches, NULL /*realm*/,
						qtokens, &etokens,
						100000 /*maxhits*/);
//		FT->dbg(FT,"%s: %lu", query, hits);
		free(qtokens);
		/* currently matches is unused */
	}
		
	stime=stopwatch_free_elapsed (gsw);

	/* remove */
	gsw = stopwatch_new (TRUE);
	for(i=0;i<maxhosts;i++) {
		StopWatch *sw;
		FTSHost *shost=hostlist[i].host;
		sw = stopwatch_new (TRUE);
		if (!ft_search_db_remove_host(shost,dummy))
			FT->err(FT,"error removing host %s", net_ip_str(shost->host));
		FT->dbg (FT, "delete %s(%lu): %.06fs elapsed", net_ip_str (shost->host),
				 shost->shares, stopwatch_free_elapsed (sw));
		ft_search_db_sync (shost);
	}

	rtime=stopwatch_free_elapsed (gsw);
	FT->dbg (FT, "insert(%lu): %.06fs elapsed (avg %.02f files/s)", files, itime, files/itime);
	FT->dbg (FT, "search(%lu): %.06fs elapsed (avg %.02f queries/s)", nqueries, stime, nqueries/stime);
	FT->dbg (FT, "remove(%lu): %.06fs elapsed (avg %.02f files/s)", files, rtime, files/rtime);

	return 0;
}

int main (int argc, char **argv)
{
	Protocol p;
	FT=&p;
        FT->trace           = trace_wrapper;
        FT->dbg             = dbg_wrapper;
        FT->err           = err_wrapper;
        FT->warn             = warn_wrapper;
        FT->udata             = NULL;
        FT->name             = "db_bench";

	log_init(GLOG_STDERR, NULL, 0, 0, NULL);

	!hash_algo_register (FT, "MD5", HASH_PRIMARY, dummy, dummy);

	if (argc>1)
		maxhosts=atoi(argv[1]);

	FT->dbg(FT,"maxhosts=%d",maxhosts);

	if (load_test_data("test.data"))
		return 1;

	if (load_queries("queries"))
		return 1;

	ft_search_db_init ();

	
	do_benchmarks();

	return 0;
}

/* compile with:
 * gcc -DHAVE_CONFIG_H -DBENCHMARK db_bench.c -I. -I.. -I../.. -I../../lib -I../../src -I../../plugin md5.o ../ft_share_file.o ../../src/share_file.o ../../src/share_hash.o ../../src/meta*.o ../../src/mime.o ../ft_shost.o -logg -lvorbis -lvorbisfile -I/usr/include -ldb -lgiFT -o db_bench
 */
