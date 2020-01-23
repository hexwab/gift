#ifndef __giFTINTERVAL_H__
#define __giFTINTERVAL_H__

#include "giFT.h"

/**
 * A data structure representing the status of a chunk of a file.
 * This chunk represents the section with bytes start through end-1.
 * status indicates the number of outstanding requests for this chunk
 * (if >= 0), or that the chunk has been successfully downloaded (if -1).
 * If should always be the case that:
 *   if prev == NULL, then start == 0
 *   if prev != NULL, then prev->end == start
 *   if next == NULL, then end == filesize
 *   if next != NULL, then next->start == end
 */
typedef struct s_Interval {
    size_t start;
    size_t end;
    int status;
    struct s_Interval *prev, *next;
} Interval;

/**
 * The kind of update we pass to intervalUpdate.
 */
typedef enum { INTERVAL_RECEIVED, INTERVAL_INC_REQUESTED,
    INTERVAL_DEC_REQUESTED } IntervalUpdateMode;

/**
 * Construct a new set of Intervals, corresponding to a file of the
 * given size.  The status is initially 0 (unrequested).
 */
Interval *intervalNew(size_t filesize);

/**
 * Free the memory associated with a list of Intervals.  Hand over the
 * first Interval in the list.
 */
void intervalFree(Interval **intvp);

/**
 * Update the Intervals according to the given parameters.
 */
int intervalUpdate(Interval *intv, size_t start, size_t end,
	IntervalUpdateMode mode,
	void (*rcvd)(size_t start, size_t len, void *context),
	void *rcvd_context);

#endif
