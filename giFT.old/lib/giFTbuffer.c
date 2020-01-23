#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "giFTbuffer.h"

/**
 * Initialize a Buffer.
 */
void bufferNew(Buffer *bp)
{
    if (bp == NULL) return;
    bp->data = NULL;
    bp->pos = 0;
    bp->alloc = 0;
    bp->valid = 1;
}

/**
 * Free the memory allocated to a Buffer.
 */
void bufferFree(Buffer *bp)
{
    if (bp == NULL) return;
    if (bp->data) free(bp->data);
    bp->data = NULL;
    bp->pos = 0;
    bp->alloc = 0;
    bp->valid = 1;
}

/**
 * Write data into a Buffer.
 */
void bufferWriteData(Buffer *bp, byte *data, size_t len)
{
    size_t newlen;

    if (bp == NULL || data == NULL || len == 0) return;

    if (!bp->valid) return;

    newlen = bp->pos + len;
    if (newlen > bp->alloc) {
	size_t newalloc = newlen + 1000;
	byte *newdata = realloc(bp->data, newalloc);
	if (!newdata) {
	    bp->valid = 0;
	    return;
	}
	bp->data = newdata;
	bp->alloc = newalloc;
    }
    memmove(bp->data + bp->pos, data, len);
    bp->pos = newlen;
}

/**
 * Write an encoded unsigned integer into a Buffer.
 */
void bufferWriteEncodedInt(Buffer *bp, size_t v)
{
    byte buf[10];
    int pos = 10;
    byte lastflag = 0x00;
    
    if (bp == NULL) return;
    do {
	buf[--pos] = lastflag | (v & 0x7f);
	v >>= 7;
	lastflag = 0x80;
    } while (v > 0);

    bufferWriteData(bp, buf+pos, 10-pos);
}
