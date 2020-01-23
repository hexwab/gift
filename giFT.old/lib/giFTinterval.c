#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "giFTinterval.h"

/**
 * Construct a new set of Intervals, corresponding to a file of the
 * given size.  The status is initially 0 (unrequested).
 */
Interval *intervalNew(size_t filesize)
{
    Interval *retval = malloc(sizeof(Interval));
    if (!retval) return NULL;
    retval->start = 0;
    retval->end = filesize;
    retval->status = 0;
    retval->prev = NULL;
    retval->next = NULL;
    return retval;
}

/**
 * Free the memory associated with a list of Intervals.  Hand over the
 * first Interval in the list.
 */
void intervalFree(Interval **intvp)
{
    Interval *intv;

    if (intvp == NULL || *intvp == NULL) return;

    intv = *intvp;
    while(intv) {
	Interval *next = intv->next;
	free(intv);
	intv = next;
    }
    *intvp = NULL;
}

/**
 * Update the Intervals according to the given parameters.
 */
int intervalUpdate(Interval *intv, size_t start, size_t end,
	IntervalUpdateMode mode,
	void (*rcvd)(size_t start, size_t len, void *context),
	void *rcvd_context)
{
    Interval *origintv = intv;

    /* Find the Interval containing the beginning of this chunk. */
    while(intv && intv->end <= start) {
	intv = intv->next;
    }
    if (!intv) return -1;

    /* Now we know that intv->start <= start < intv->end. */

    /* Break this Interval at the start position, if necessary. */
    if (intv->start < start) {
	Interval *newintv = malloc(sizeof(Interval));
	if (!newintv) return -1;
	newintv->start = start;
	newintv->end = intv->end;
	newintv->status = intv->status;
	newintv->prev = intv;
	newintv->next = intv->next;
	intv->next = newintv;
	intv->end = start;
	intv = newintv;
    }

    /* Now we know that intv->start == start < intv->end. */

    while(intv && intv->start < end) {
	if (intv->end > end) {
	    /* Break this Interval at the end position. */
	    Interval *newintv = malloc(sizeof(Interval));
	    if (!newintv) return -1;
	    newintv->start = end;
	    newintv->end = intv->end;
	    newintv->status = intv->status;
	    newintv->prev = intv;
	    newintv->next = intv->next;
	    intv->next = newintv;
	    intv->end = end;
	}
	/* This whole interval gets changed. */
	switch(mode) {
	    case INTERVAL_RECEIVED:
		if (intv->status >= 0) {
		    intv->status = -1;
		    if (rcvd) {
			rcvd(intv->start, intv->end - intv->start,
				rcvd_context);
		    }
		}
		break;
	    case INTERVAL_INC_REQUESTED:
		if (intv->status >= 0) {
		    intv->status += 1;
		}
		break;
	    case INTERVAL_DEC_REQUESTED:
		if (intv->status >= 1) {
		    intv->status -= 1;
		}
		break;
	}
	intv = intv->next;
    }

    /* Now compress the list, if possible. */
    for (intv = origintv; intv; intv = intv -> next) {
	while (intv->next && intv->status == intv->next->status) {
	    Interval *next = intv->next;
	    intv->end = next->end;
	    intv->next = next->next;
	    if (intv->next) intv->next->prev = intv;
	    free(next);
	}
    }

    return 0;
}
