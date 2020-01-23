#ifndef __giFTDOWNLOAD_H__
#define __giFTDOWNLOAD_H__

typedef struct s_DownloadContext *DownloadContext;

#include "giFTpacket.h"

typedef struct {
    size_t recv, filesize, percent;
} DownloadStats;

/**
 * This is the function that determines what ranges to ask each
 * Connection to a source for.  This algorithm could be improved.
 * It also is used to report chunks of the file we've received.
 */
int downloadHandler(int which, DownloadContext dc, size_t dlid, char **urip,
	size_t *startp, size_t *lenp, byte *buf, void *drh_context);

/**
 * Create a new DownloadContext, in anticipation of downloading a file
 * of the specified size, to be written to the given FILE *.  Use the
 * given array of Connections to contact the sources.
 */
DownloadContext downloadNewContext(size_t filesize, FILE *writef,
	Connection *sources, size_t nsources);

/**
 * Free all resources allocated by a DownloadContext.  This will
 * terminate the download if it is in progress.
 */
void downloadFreeContext(DownloadContext *dcp);

/**
 * Add a potential source to a DownloadContext.
 */
int downloadAddSource(DownloadContext dc, struct sockaddr_in *sin,
	char *uri);

/**
 * Is this download finished?
 */
int downloadIsDone(DownloadContext dc);

/**
 * Get statstics for a current DownloadContext.
 */
void downloadGetStats(DownloadContext dc, DownloadStats *dsp);

/**
 * Return the next Interval containing received data.
 */
void *downloadGetFilled(DownloadContext dc, void *ptr,
    size_t *startp, size_t *endp);

/**
 * Mark the given Interval as having been received.
 */
size_t downloadSetFilled(DownloadContext dc, size_t start, size_t end);

#endif
