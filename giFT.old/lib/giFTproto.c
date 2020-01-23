#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include "giFTpacket.h"
#include "giFTproto.h"

/**
 * An array of strings with human-readable versions of the above types.
 */
const char *statsFileTypeName[NumStatsFileType] = {
    "Misc", "Audio", "Video", "Images", "Documents", "Software"
};

/**
 * Initialize the giFT library.
 */
void giFTInit(void)
{
    /* We need to ignore SIGPIPE */
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
}

/**
 * Read a multibyte integer from the given buffer.  The passed pointers
 * to the current buffer position and its length are updated to reflect
 * the read integer.  The unsigned integer read is returned.  See the
 * README file for the format of these multibyte integers.
 */
static size_t read_int(byte **bufp, size_t *lenp)
{
    size_t retval = 0;
    while(*lenp > 0) {
	byte nextb = **bufp;
	retval <<= 7;
	retval |= (nextb&0x7f);
	++(*bufp);
	--(*lenp);
	if (nextb < 0x80) break;
    }
    return retval;
}

/**
 * Parse a packet containing a list of peers (types 0x00 and 0x02).
 * The supplied AddressHandler callback will be called once for each
 * peer found in the list.
 */
void readNodeList(int type, size_t size, byte *buf, AddressHandler ah,
	void *ah_context)
{
    if (type == 0x02 || type == 0x00) {
	size_t i;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	for(i=0;i<size/8;++i) {
	    addr.sin_family = AF_INET;
	    addr.sin_port = htons((buf[8*i+4]<<8)|buf[8*i+5]);
	    addr.sin_addr.s_addr = htonl( (buf[8*i]<<24) | (buf[8*i+1]<<16) |
		    (buf[8*i+2]<<8) | buf[8*i+3]);
	    ah(-1, (struct sockaddr *)&addr, addrlen, ah_context);
	}
    }
}

/**
 * Send a query packet with the given query id for the given string.
 * The string must be less than 16K in length.  The query id should
 * always be non-zero.
 */
int sendQuery(Connection c, unsigned short id, char *query)
{
    size_t querylen, querylenlen;
    byte *outmsg;

    if (c == NULL || query == NULL || query[0] == '\0') return -1;

    querylen = strlen(query);
    if (querylen <= 127) {
	querylenlen = 1;
    } else if (querylen <= 16383) {
	querylenlen = 2;
    } else {
	return -1;
    }
    outmsg = malloc(querylen+querylenlen+11);
    if (outmsg == NULL) {
	fprintf(stderr, "Out of memory at %s:%d\n", __FILE__, __LINE__);
	return -1;
    }

    /* The last 2 bytes in the following are the max number of search
     * results to return. */
    memmove(outmsg, "\x00\x01\x01\x00", 4);
    outmsg[4] = (id >> 8) & 0xff;
    outmsg[5] = id & 0xff;
    memmove(outmsg+6, "\x01\x3f\x01\x05\x00", 5);
    if (querylen <= 127) {
	outmsg[11] = querylen;
    } else {
	outmsg[11] = 0x80 | (querylen>>7);
	outmsg[12] = querylen & 0x7f;
    }
    memmove(outmsg+11+querylenlen, query, querylen);

    writePacket(c, 6, outmsg, 11+querylen+querylenlen);

    free(outmsg);

    return 0;
}

/**
 * Compile a given ComplexQuery for later sending.  Be sure to free()
 * the result when you're done.
 */
byte* compileComplexQuery(ComplexQuery cq, size_t *outmsglenp)
{
    byte *outmsg = NULL;
    size_t outmsglen = 0;
    size_t i;
    size_t numterms;
    byte *ptr;

    if (cq == NULL) return NULL;

    /* Figure out how big this message is going to be */
    outmsglen = 9;  /* The header */
    numterms = cq->numterms;
    if (numterms > 127) numterms = 127;

    for(i=0;i<numterms;++i) {
	size_t len;
	outmsglen += 2;  /* The term type and field */
	len = cq->terms[i].len;
	if (len < 128) {
	    outmsglen += 1 + len;
	} else if (len < 128*128) {
	    outmsglen += 2 + len;
	} else if (len < 128*128*128) {
	    outmsglen += 3 + len;
	} else if (len < 128*128*128*128) {
	    outmsglen += 4 + len;
	} else {
	    outmsglen += 5 + len;
	}
    }

    /* OK; allocate the buffer */
    outmsg = malloc(outmsglen);
    if (outmsg == NULL) return NULL;

    /* Now start building the message */

    /* We don't know what these first two bytes are */
    outmsg[0] = 0x00;
    outmsg[1] = 0x01;

    outmsg[2] = cq->maxresults >> 8;
    outmsg[3] = cq->maxresults & 0xff;

    outmsg[4] = cq->qid >> 8;
    outmsg[5] = cq->qid & 0xff;

    /* We don't know what this byte is for */
    outmsg[6] = 0x01;

    outmsg[7] = cq->realm;
    outmsg[8] = numterms;

    ptr = outmsg+9;
    /* Build each search term */
    for(i=0;i<numterms;++i) {
	QueryTerm *qt = cq->terms + i;
	size_t len = qt->len;
	ptr[0] = qt->type;
	ptr[1] = qt->field;
	if (len < 128) {
	    ptr[2] = len;
	    ptr += 3;
	} else if (len < 128*128) {
	    ptr[3] = len & 0x7f;
	    len >>= 7;
	    ptr[2] = len | 0x80;
	    ptr += 4;
	} else if (len < 128*128*128) {
	    ptr[4] = len & 0x7f;
	    len >>= 7;
	    ptr[3] = (len&0x7f) | 0x80;
	    len >>= 7;
	    ptr[2] = len | 0x80;
	    ptr += 5;
	} else if (len < 128*128*128*128) {
	    ptr[5] = len & 0x7f;
	    len >>= 7;
	    ptr[4] = (len&0x7f) | 0x80;
	    len >>= 7;
	    ptr[3] = (len&0x7f) | 0x80;
	    len >>= 7;
	    ptr[2] = len | 0x80;
	    ptr += 6;
	} else {
	    ptr[6] = len & 0x7f;
	    len >>= 7;
	    ptr[5] = (len&0x7f) | 0x80;
	    len >>= 7;
	    ptr[4] = (len&0x7f) | 0x80;
	    len >>= 7;
	    ptr[3] = (len&0x7f) | 0x80;
	    len >>= 7;
	    ptr[2] = len | 0x80;
	    ptr += 7;
	}
	len = qt->len;  /* We destroyed the original value above */
	if (len > 0) memmove(ptr, qt->data, len);
	ptr += len;
    }

    if (outmsglenp) *outmsglenp = outmsglen;
    return outmsg;
}

/**
 * Send a compiled query packet to the given Connection.
 */
int sendComplexQuery(Connection c, byte *compiled_query, size_t len)
{
    if (c == NULL || compiled_query == NULL) return -1;

    writePacket(c, 6, compiled_query, len);

    return 0;
}

/**
 * Free the memory associated with a FileEntry record.
 */
static void freeFileEntry(FileEntry *fep)
{
    size_t i;
    if (fep == NULL) return;

    if (fep->username) free(fep->username);
    if (fep->networkname) free(fep->networkname);
    if (fep->d.tags) {
	for(i=0;i<fep->d.ntags;++i) {
	    if (fep->d.tags[i].data) free(fep->d.tags[i].data);
	}
	free(fep->d.tags);
    }
}

/**
 * Free the memory associated with a QueryResponse object.  Pass the
 * address of the variable storing the result returned from
 * readQueryResponse.  The memory will be freed, and the variable will
 * be set to NULL.
 */
void freeQueryResponse(QueryResponse *qrp)
{
    size_t i;
    if (qrp == NULL || *qrp == NULL) return;

    for(i=0;i<(*qrp)->numresp;++i) {
	freeFileEntry((*qrp)->files + i);
    }
    if ((*qrp)->files) free((*qrp)->files);
    free(*qrp);
    *qrp = NULL;
}

/**
 * Parse a packet containing a response to a query (type 0x07).  This
 * routine will pass back a pointer to a newly allocated QueryResponse
 * object.  Be sure to call freeQueryResponse when you're done with it.
 */
QueryResponse readQueryResponse(int type, size_t size, byte *buf)
{
    QueryResponse qr = NULL;
    size_t resnum;
    
    if (type != 0x07 || size < 10) return NULL;
    qr = malloc(sizeof(struct s_QueryResponse));
    if (qr == NULL) return NULL;

    /* Read the QueryResponse header from the packet */
    qr->addr.sin_family = AF_INET;
    qr->addr.sin_port = htons((buf[4] << 8) | buf[5]);
    qr->addr.sin_addr.s_addr = htonl((buf[0]<<24)|(buf[1]<<16)|
	    (buf[2]<<8)|buf[3]);
    qr->id = (buf[6]<<8)|buf[7];
    qr->numresp = (buf[8]<<8)|buf[9];
    if (qr->numresp > 0) {
	/* Allocate the appropriate number of FileEntry records. */
	qr->files = calloc(qr->numresp, sizeof(FileEntry));
	if (qr->files == NULL) {
	    free(qr);
	    return NULL;
	}
    } else {
	qr->files = NULL;
    }

    buf += 10;
    size -= 10;
    for(resnum = 0; resnum < qr->numresp && size > 0; ++resnum) {
	FileEntry *fe = &(qr->files[resnum]);
	size_t tagnum;

	if (size < 8) break;
	fe->host.sin_family = AF_INET;
	fe->host.sin_port = htons((buf[4] << 8) | buf[5]);
	fe->host.sin_addr.s_addr = htonl((buf[0]<<24)|(buf[1]<<16)|
		(buf[2]<<8)|buf[3]);
	fe->bandwidthtag = buf[6];
	fe->username = NULL;
	fe->networkname = NULL;
	fe->d.ntags = 0;
	fe->d.tags = NULL;

	/* Parse the username and networkname (aka client software name).
	 * If the first byte is \x02, then use the address to look up
	 * the username and networkname from a previous result.
	 * Otherwise, the username is terminated with \x01, and then the
	 * networkname is terminated with \x00.
	 */
	if (buf[7] == '\x02') {
	    /* This is a duplicate from one we've seen before.  Try
	     * to find it. */
	    size_t i;
	    for(i=0;i<resnum;++i) {
		if (qr->files[i].host.sin_addr.s_addr ==
			fe->host.sin_addr.s_addr &&
			qr->files[i].host.sin_port == fe->host.sin_port) {
		    fe->username = strdup(qr->files[i].username);
		    fe->networkname = strdup(qr->files[i].networkname);
		    break;
		}
	    }
	    if (fe->username == NULL) fe->username = strdup("UNKNOWN");
	    if (fe->networkname == NULL) fe->networkname = strdup("UNKNOWN");
	    buf += 8;
	    size -= 8;
	} else {
	    byte *dot;
	    size_t slen;

	    buf += 7;
	    size -= 7;
	    
	    dot = strchr(buf, '\x01');
	    if (dot) {
		slen = dot-buf;
		if (size < slen+1) break;
		*dot = '\0';
		fe->username = strdup(buf);
		buf += (slen+1);
		size -= (slen+1);
	    } else {
		fe->username = strdup("UNKNOWN");
	    }
	    slen = strlen(buf);
	    if (size < slen+1) break;
	    fe->networkname = strdup(buf);
	    buf += (slen+1);
	    size -= (slen+1);
	}

	if (size < 0x17) break;
	/* The 20 byte hash */
	memmove(fe->d.hash, buf, 20);
	buf += 0x14;
	size -= 0x14;
	/* Read some metadata associated with this file */
	fe->d.checksum = read_int(&buf, &size);
	if (size == 0) break;
	fe->d.filesize = read_int(&buf, &size);
	if (size == 0) break;
	fe->d.ntags = read_int(&buf, &size);
	if (size == 0) break;
	fe->d.tags = calloc(fe->d.ntags, sizeof(FileTag));
	if (fe->d.tags == NULL) {
	    freeQueryResponse(&qr);
	    return NULL;
	}
	for(tagnum=0; tagnum < fe->d.ntags && size > 0; ++tagnum) {
	    FileTag *ft = &(fe->d.tags[tagnum]);
	    ft->tag = 0;
	    ft->len = 0;
	    ft->data = NULL;
	    if (size < 2) break;
	    ft->tag = read_int(&buf, &size);
	    if (size == 0) break;
	    ft->len = read_int(&buf, &size);
	    if (size < ft->len) break;
	    ft->data = malloc(ft->len + 1);
	    if (ft->data) {
		if (ft->len > 0) memmove(ft->data, buf, ft->len);
		ft->data[ft->len] = '\0';
	    }
	    buf += ft->len;
	    size -= ft->len;
	}

	/* OK, we're done with this one. */
	fe->valid = 1;
    }

    if (size != 0) {
	fprintf(stderr, "Weird size remains: %u\n", size);
    }

    return qr;
}

/**
 * Parse a "no more search results" packet (type 0x08).  This routine
 * returns the relevant query id.
 */
unsigned short readQueryTerminated(int type, size_t size, byte *buf)
{
    if (type != 0x08 || size != 2 || buf == NULL) return 0;
    return ((buf[0]<<8)|buf[1]);
}

/**
 * Create a Complex Query, initially with no query terms.
 */
ComplexQuery createComplexQuery(unsigned short maxresults, unsigned short qid)
{
    ComplexQuery retval = NULL;

    retval = malloc(sizeof(struct s_ComplexQuery));
    if (!retval) return NULL;

    retval->maxresults = maxresults;
    retval->qid = qid;
    retval->realm = QUERY_REALM_EVERYTHING;
    retval->numterms = 0;
    retval->terms = NULL;

    return retval;
}

/**
 * Free the memory allocated to a complex query.
 */
void freeComplexQuery(ComplexQuery *cqp)
{
    size_t i;

    if (cqp == NULL || *cqp == NULL) return;
    for(i=0;i<(*cqp)->numterms;++i) {
	QueryTerm *qt = (*cqp)->terms + i;
	if (qt->data) free(qt->data);
    }
    if ((*cqp)->terms) free((*cqp)->terms);
    free(*cqp);
    *cqp = NULL;
}

/**
 * Add a QueryTerm to a complex query.  Returns the new number of terms
 * in the query, or -1 on error.
 */
int buildComplexQuery(ComplexQuery cq, QueryTermType type, byte field,
	size_t len, byte *data)
{
    QueryTerm *newterms;
    byte *newdata;

    if (cq == NULL) return -1;

    /* We won't let you mave more than 127 terms */
    if (cq->numterms == 127) {
	return 127;
    }

    /* Copy the data */
    if (len > 0) {
	newdata = malloc(len);
	if (newdata == NULL) return -1;
	memmove(newdata, data, len);
    } else {
	newdata = NULL;
    }

    /* Make room for a new QueryTerm */
    newterms = realloc(cq->terms, (cq->numterms+1)*sizeof(QueryTerm));
    if (!newterms) {
	free(newdata);
	return -1;
    }

    cq->terms = newterms;
    cq->terms[cq->numterms].type = type;
    cq->terms[cq->numterms].field = field;
    cq->terms[cq->numterms].len = len;
    cq->terms[cq->numterms].data = newdata;
    cq->numterms++;

    return cq->numterms;
}
