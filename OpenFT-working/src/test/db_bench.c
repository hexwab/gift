#include "ft_openft.h"
#include "meta.h"
#include "libgift/proto/protocol.h"
#include "plugin.h"
#include "ft_conf.h"
#include "md5.h"
#include "libgift/array.h"
#include "libgift/stopwatch.h"
#include "ft_search_db.c"

Protocol *FT=NULL;



/* remove shares host-at-a-time instead of file-at-a-time */
#define REMOVE_BY_HOST 

/* remove hosts in reverse order to how they were added */
/* #define REVERSE_REMOVAL */

#define SEARCH
  
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

char *ft_node_fmt (FTNode *node)
{
	/* keep crappy printf implementations happy */
	if (!node)
		return "(null)";

	return stringf ("%s:%hu", net_ip_str (node->ip), node->port);
}

FTNode *ft_node_new (in_addr_t ip)
{
	FTNode *node;

	if (!(node = MALLOC (sizeof (FTNode))))
		return NULL;

	node->ip = ip;
	node->state = FT_NODE_DISCONNECTED;
	node->klass = FT_NODE_USER;

	return node;
}

static FTSession *create_session (FTNode *node)
{
	FTSession *s;

	if (!node)
		return NULL;

	/* even if we already have a session, we still want to memset everything */
	if (!(s = node->session))
	{
		if (!(s = malloc (sizeof (FTSession))))
			return NULL;
	}

	memset (s, 0, sizeof (FTSession));
	node->session = s;

	return s;
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

uint32_t *ft_search_tokenize (const char *string)
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

/************************************************************************/

struct hl {
	FTNode *node;
	Array *files;
} *nodelist;
int nodes=0, files=0;
int maxnodes=100;
int maxqueries=100000;
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
			FT->dbg(FT, "test db contains %d nodes", nodes=stats->bt_ndata);
		else
		{
			FT->err(FT, "failed to get test db stats");
			return 1;
		}
		if (maxnodes>nodes)
			maxnodes=nodes;
		nodelist=malloc(nodes*sizeof(struct hl));

		if (test_db->cursor (test_db, NULL, &cur, 0))
			FT->err(FT,"cursor init failed");
		
		memset (&key, 0, sizeof (key));
		memset (&value, 0, sizeof (value));

		while ((ret = cur->c_get (cur, &key, &value, DB_NEXT)) == 0) {
			DB *db;
			FTNode *node;
			char hname[256];
			struct hl hl;
			int hfiles=0;

			memcpy(hname, key.data, key.size);
			hname[key.size]=0;
			
//			FT->dbg(FT, "adding node %s", hname);
			node=ft_node_new (net_ip (hname));
			create_session (node);
			search_db_new (node);
			db=bm_open_db(db_name, hname);
			if (!db)
				FT->err(FT, "failed to open db for node %s", hname);
			if (!node)
				FT->err(FT, "failed to create node for %s",hname);
			hl.node=node;
			if (!db->stat(db, &stats, 0))
				hfiles=stats->bt_ndata;
			else
				FT->err(FT, "failed to get stats for %s", hname);
			hl.files=array_new(NULL);
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
					file=unserialize_record(FT_SEARCH_DB(node), md5, fvalue.data);
//					FT->dbg(FT, "filename=%s",file->path);
					if (!file)
						FT->DBGFN(FT, "unserialize_record failed");
					else
						array_push(&hl.files, file);
				}
				if (fret != DB_NOTFOUND)
					FT->err (FT, "DBcursor->c_get: %d", fret);
			}
			nodelist[i++]=hl;

			bm_close_db(db);
			if (i>maxnodes)
				break;
		}

		nodes=i-1;

		FT->dbg(FT, "loaded %d total files (average %d)", files, files/nodes);

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
	int n=0;
	
	queries=array_new (NULL);

	if (!(f=fopen(filename, "r")))
	{
		FT->err(FT, "error reading search queries");
		return 1;
	}

	while (fgets(buf, sizeof(buf), f) && ++n<maxqueries)
	{
		buf[strlen(buf)-1]=0; /* chop */
		if (!array_push(&queries, strdup(buf)))
			FT->err(FT,"error adding search query");
	}

	fclose(f);
	FT->dbg(FT, "loaded %d search queries", array_count(&queries));
	return 0;
}

void dummy (FTNode *f) 
{
}

int do_benchmarks() {
	int i,j;
	StopWatch *gsw;
	double itime,rtime,stime;
	int nqueries=array_count(&queries);

	/* insert */
	gsw = stopwatch_new (TRUE);
	for(i=0;i<maxnodes;i++) {
		StopWatch *sw;
		FTNode *node=nodelist[i].node;
		Array *files=nodelist[i].files;
		int flen=array_count(&files);
//		sw = stopwatch_new (TRUE);
		FT->dbg(FT, "inserting %d shares from %s",flen,ft_node_fmt(node));
		for(j=0;j<flen;j++) {
			FileShare *file=array_splice (&files, j, 0, NULL);
			if (file) {
				if (!ft_search_db_insert(node,file))
					FT->err(FT,"error inserting file %s (%s)", file->path, ft_node_fmt(node));
			} else 
				FT->err(FT, "error reading file array");
		}
//			FT->dbg (FT, "%s(%lu): %.06fs elapsed", ft_node_fmt (node),
//				 shost->shares, stopwatch_free_elapsed (sw));
		ft_search_db_sync (node);
	}

	//	exit(0);
	itime=stopwatch_free_elapsed (gsw);

#ifdef REMOVE_BY_HOST
	/* remove the file list to save memory */
	{
		StopWatch *sw;
		sw = stopwatch_new (TRUE);
		for(i=0;i<maxnodes;i++) {
			FTNode *node=nodelist[i].node;
			Array *files=nodelist[i].files;
			int flen=array_count(&files);
			for(j=0;j<flen;j++) {
				FileShare *file=array_splice (&files, j, 0, NULL);
				ft_share_unref (file);
			}
			array_unset (&files);
		}
		FT->dbg (FT, "free: %.06fs elapsed", stopwatch_free_elapsed (sw));
	}
#endif

	system ("ls -oL benchtemp");
#ifdef SEARCH
	/* search */
	gsw = stopwatch_new (TRUE);
	for(i=0;i<nqueries;i++) {
		char *query=array_splice (&queries, i, 0, NULL);
		uint32_t *qtokens;
		uint32_t etokens=0;
		Array *matches=NULL;
		int hits;
		int j;

		qtokens = ft_search_tokenize (query);
		if (!qtokens)
			FT->err(FT, "error tokenizing '%s'", query);
		hits = ft_search_db_tokens (&matches, NULL /*realm*/,
						qtokens, &etokens,
						1000 /*maxhits*/);
//		FT->dbg(FT,"%s: %lu", query, hits);
		free(qtokens);
		/* currently matches is unused */
		for (j=0; j<hits; j++)
			ft_share_unref (array_pop (&matches));
		array_unset (&matches);
	}
		
	stime=stopwatch_free_elapsed (gsw);
#endif
	/* remove */
	gsw = stopwatch_new (TRUE);

#ifndef REVERSE_REMOVAL
	for(i=0;i<maxnodes;i++) { /* FIFO */
#else
	for(i=maxnodes-1;i;i--) { /* LIFO */
#endif

		StopWatch *sw;
		FTNode *node=nodelist[i].node;

#ifndef REMOVE_BY_HOST
		/* shares gets removed by db_remove_host */
		unsigned long shares=FT_SEARCH_DB(node)->shares; 
		Array *f=nodelist[i].files;
		int flen=array_count(&f);
		sw = stopwatch_new (TRUE);
		for(j=0;j<flen;j++) {
			FileShare *file=array_splice (&f, j, 0, NULL);
			Hash *hash=share_get_hash (file, "MD5");
			if (hash) {
				if (!ft_search_db_remove(node,hash->data))
					FT->err(FT,"error removing file %s (%s)", file->path, ft_node_fmt(node));
			} else 
				FT->err(FT, "error reading file array");
		}
#else
		/* shares gets removed by db_remove_host */
		unsigned long shares=FT_SEARCH_DB(node)->shares; 

		sw = stopwatch_new (TRUE);
		if (!ft_search_db_remove_host(node))
			FT->err(FT,"error removing node %s", ft_node_fmt(node));
#endif

		FT->dbg (FT, "delete %s(%lu): %.06fs elapsed", ft_node_fmt (node),
				 shares, stopwatch_free_elapsed (sw));
		ft_search_db_sync (node);
	}

	rtime=stopwatch_free_elapsed (gsw);
	FT->dbg (FT, "insert(%lu): %.06fs elapsed (avg %.02f files/s)", files, itime, files/itime);
#ifdef SEARCH
	FT->dbg (FT, "search(%lu): %.06fs elapsed (avg %.02f queries/s)", nqueries, stime, nqueries/stime);
#endif
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
        FT->hashes            = NULL;
	FT->udata = malloc(sizeof(OpenFT));
	((OpenFT *)FT->udata)->conf = NULL;

	log_init(GLOG_STDERR, NULL, 0, 0, NULL);

	hash_algo_register (FT, "MD5", HASH_PRIMARY, dummy, dummy);

	if (argc>1)
		maxnodes=atoi(argv[1]);

	if (argc>2)
		maxqueries=atoi(argv[2]);

	FT->dbg(FT,"maxnodes=%d",maxnodes);

	if (load_test_data("test.data"))
		return 1;

	if (load_queries("queries"))
		return 1;

	ft_search_db_init ("benchtemp");

	
	do_benchmarks();

	return 0;
}

/* compile with:
 * gcc -g -Wall -DHAVE_CONFIG_H -DBENCHMARK db_bench.c -I. -I.. -I../.. -I../../lib -I../../src -I../../plugin ../md5.o ../ft_share_file.o ../../src/meta.o ../../src/meta/meta*.c ../ft_conf.c ../../src/mime.o -logg -lvorbis -lvorbisfile -ldb -lgift -lgiftproto -o db_bench
 */
