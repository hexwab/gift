#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "giFTinterval.h"
#include "giFTdownload.h"

typedef struct {
    char *uri;
    size_t index;
    size_t start, end;
} DownloadStatus;

struct s_DownloadContext {
    size_t filesize;
    size_t recv;
    FILE *writef;
    Connection *sources;
    size_t nsources;
    DownloadStatus *status;
    size_t nstatus, statusalloc;
    Interval *intv;
};

/**
 * The context for the rcvd_bytes function.
 */
typedef struct {
    DownloadContext dc;
    byte *buf;
    size_t bufstart;
} RBContext;

/**
 * When we receive bytes we've never seen before, call this routine.
 */
static void rcvd_bytes(size_t start, size_t len, void *context)
{
    RBContext *rbc = context;
    int res;

    if (rbc == NULL || rbc->dc == NULL) return;

    rbc->dc->recv += len;

    if (rbc->dc->writef == NULL || rbc->buf == NULL) return;

    res = fseek(rbc->dc->writef, start, SEEK_SET);
    if (res < 0) return;
    fwrite(rbc->buf + start - rbc->bufstart, 1, len, rbc->dc->writef);
}

/**
 * This is the function that determines what ranges to ask each
 * Connection to a source for.  This algorithm could be improved.
 * It also is used to report chunks of the file we've received.
 */
int downloadHandler(int which, DownloadContext dc, size_t dlid, char **urip,
	size_t *startp, size_t *lenp, byte *buf, void *drh_context)
{
    DownloadStatus *dls;
    Connection c;
    size_t start, len;
    RBContext rbc;
    Interval *intv;

    if (dc == NULL || dc->sources == NULL) return -1;
    c = dc->sources[which];
    dls = dc->status + dlid;

    if (buf == NULL) {
	/* We're being asked which chunk to do next. */

	/* See if there's an unrequested piece. */
	intv = dc->intv;
	while (intv && intv->status != 0) intv = intv->next;
	if (intv) {
	    dls->start = intv->start;
	    dls->end = intv->end;
	} else {
	    /* Find the largest remaining piece. */
	    size_t maxsize = 0;
	    Interval *maxintv = NULL;

	    for(intv = dc->intv; intv; intv = intv->next) {
		if (intv->status >= 0 && intv->end - intv->start > maxsize) {
		    maxsize = intv->end - intv->start;
		    maxintv = intv;
		}
	    }
	    if (maxintv) {
		/* Get the second half of it, if it's big. */
		if (maxsize > 5000) {
		    dls->start = (maxintv->start + maxintv->end) / 2;
		} else {
		    dls->start = maxintv->start;
		}
		dls->end = maxintv->end;
	    } else {
		/* No pieces at all? */
		dls->start = 0;
		dls->end = dc->filesize;
	    }
	}

	if (urip) *urip = dls->uri;
	if (startp) *startp = dls->start;
	if (lenp) *lenp = dls->end - dls->start;

	intervalUpdate(dc->intv, dls->start, dls->end, INTERVAL_INC_REQUESTED,
		NULL, NULL);

	return 0;
    }

    if (startp == NULL || lenp == NULL) {
	/* We've stopped requesting this chunk of data. */
	intervalUpdate(dc->intv, dls->start, dls->end, INTERVAL_DEC_REQUESTED,
		NULL, NULL);
	return 0;
    }

    /* We were given a chunk of data.  Process it, and then return < 0
     * if we want to stop getting data from this source. */

    start = *startp;
    len = *lenp;

    rbc.dc = dc;
    rbc.buf = buf;
    rbc.bufstart = start;

    intervalUpdate(dc->intv, start, start+len, INTERVAL_RECEIVED, rcvd_bytes,
	    &rbc);

    for(intv = dc->intv ; intv ; intv = intv->next ) {
	if (intv->start <= start && intv->status == -1 &&
		(intv->next == NULL || intv->end > start+len+20000) ) {
	    /* We're receiving data we already had, so stop fetching
	     * from here. */
	    return -1;
	}
    }
    return 0;
}

/**
 * Create a new DownloadContext, in anticipation of downloading a file
 * of the specified size, to be written to the given FILE *.  Use the
 * given array of Connections to contact the sources.
 */
DownloadContext downloadNewContext(size_t filesize, FILE *writef,
	Connection *sources, size_t nsources)
{
    DownloadContext retval = NULL;
    DownloadStatus *status = NULL;

    if (filesize == 0 || writef == NULL) return NULL;

    retval = malloc(sizeof(struct s_DownloadContext));
    if (retval == NULL) return NULL;

    /* Allocate space for 10 parallel fetches initially. */
    status = calloc(10, sizeof(DownloadStatus));
    if (status == NULL) {
	free(retval);
	return NULL;
    }

    retval->intv = intervalNew(filesize);
    if (!retval->intv) {
	free(status);
	free(retval);
	return NULL;
    }
    retval->filesize = filesize;
    retval->recv = 0;
    retval->writef = writef;
    retval->sources = sources;
    retval->nsources = nsources;
    retval->status = status;
    retval->nstatus = 0;
    retval->statusalloc = 10;

    return retval;
}

/**
 * Free all resources allocated by a DownloadContext.  This will
 * terminate the download if it is in progress.
 */
void downloadFreeContext(DownloadContext *dcp)
{
    DownloadContext dc;

    if (dcp == NULL || *dcp == NULL) return;

    dc = *dcp;

    if (dc->status) {
	size_t slot;

	for(slot = 0; slot < dc->nstatus; ++slot) {
	    closeConnection(dc->sources + dc->status[slot].index);
	}
	free(dc->status);
    }
    if (dc->intv) {
	intervalFree(&(dc->intv));
    }

    free(*dcp);
    *dcp = NULL;
}

/**
 * Add a potential source to a DownloadContext.
 */
int downloadAddSource(DownloadContext dc, struct sockaddr_in *sin,
	char *uri)
{
    size_t slot;
    socklen_t sinlen;
    char *uricopy;

    if (dc == NULL || sin == NULL || sin->sin_family != AF_INET ||
	    uri == NULL || uri[0] != '/') return -1;

    uricopy = strdup(uri);
    if (uricopy == NULL) return -1;

    /* Are we already connected to this address? */
    for(slot=0; slot<dc->nstatus; ++slot) {
	if (!cmpConnectionAddr(dc->sources[dc->status[slot].index],
		    (struct sockaddr *)sin, sizeof(*sin))) {
	    return 0;
	}
    }

    /* Find a free slot. */
    for(slot=0; slot<dc->nsources; ++slot) {
	if (dc->sources[slot] == NULL) break;
    }

    if (slot == dc->nsources) return -1;

    /* Start the connection process. */
    sinlen = sizeof(*sin);
    dc->sources[slot] = newDownloadConnection(dc, slot, (struct sockaddr *)sin,
	    sinlen);
    if (dc->sources[slot] == NULL) {
	return -1;
    }

    /* Keep track of the Connections associated with this download. */
    if (dc->nstatus == dc->statusalloc) {
	size_t newalloc = dc->statusalloc + 10;
	DownloadStatus *newds = realloc(dc->status,
		newalloc * sizeof(DownloadStatus));
	if (newds == NULL) {
	    /* Uh, oh.  We can't keep track of it. */
	    closeConnection(dc->sources + slot);
	    return -1;
	}
	dc->status = newds;
	dc->statusalloc = newalloc;
    }

    dc->status[dc->nstatus].index = slot;
    dc->status[dc->nstatus].uri = uricopy;
    ++(dc->nstatus);
    return 1;
}

/**
 * Is this download finished?
 */
int downloadIsDone(DownloadContext dc)
{
    if (dc == NULL) return 1;

    if (dc->recv == dc->filesize) return 1;

    return 0;
}

/**
 * Get statstics for a current DownloadContext.
 */
void downloadGetStats(DownloadContext dc, DownloadStats *dsp)
{
    size_t r,f;

    if (dc == NULL || dsp == NULL) return;

    f = dsp->filesize = dc->filesize;
    r = dsp->recv = dc->recv;

    /* Prevent the 100*r from overflowing for large files. */
    if (dsp->filesize > 40000000) {
	f = f>>10;
	r = r>>10;
    }
    dsp->percent = (100*r) / f;
}

/**
 * Return the next Interval containing received data.
 */
void *downloadGetFilled(DownloadContext dc, void *ptr,
    size_t *startp, size_t *endp)
{
    Interval *intv;

    if (dc == NULL || startp == NULL || endp == NULL) return NULL;
    
    /* Use the supplied pointer, if present, or start at the beginning
     * otherwise. */
    intv = (Interval *)ptr;
    if(!intv) intv = dc->intv;

    /* Find the next Interval we've received. */
    while(intv) {
	if (intv->status == -1) {
	    *startp = intv->start;
	    *endp = intv->end;
	    return intv->next;
	}
	intv = intv->next;
    }

    /* There wasn't one. */
    *startp = 0;
    *endp = 0;
    return NULL;
}

/**
 * Mark the given Interval as having been received.
 */
size_t downloadSetFilled(DownloadContext dc, size_t start, size_t end)
{
    intervalUpdate(dc->intv, start, end, INTERVAL_RECEIVED, NULL, NULL);
    dc->recv += (end - start);
    return (dc->recv);
}
