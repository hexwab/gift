#ifndef __giFTBUFFER_H__
#define __giFTBUFFER_H__

#include "giFT.h"

/**
 * A growable buffer for writing into.
 */
typedef struct {
    int valid;
    byte *data;
    size_t pos;
    size_t alloc;
} Buffer;

/**
 * Initialize a Buffer.
 */
void bufferNew(Buffer *bp);

/**
 * Free the memory allocated to a Buffer.
 */
void bufferFree(Buffer *bp);

/**
 * Write data into a Buffer.
 */
void bufferWriteData(Buffer *bp, byte *data, size_t len);

/**
 * Write an encoded unsigned integer into a Buffer.
 */
void bufferWriteEncodedInt(Buffer *bp, size_t v);

#endif
