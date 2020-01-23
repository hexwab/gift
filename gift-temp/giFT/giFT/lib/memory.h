/*
 * $Id: memory.h,v 1.6 2003/05/05 11:53:47 jasta Exp $
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

#ifndef __MEMORY_H
#define __MEMORY_H

/*****************************************************************************/

/**
 * @file memory.h
 *
 * @brief Simple wrappers for the C libraries allocation functions.
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#undef CALLOC
#undef MALLOC
#undef FREE
#undef REALLOC

#ifndef TCG_LEAK_DETECT
# define CALLOC(nmemb,size)  gift_calloc(nmemb,size)
# define MALLOC(size)        CALLOC(1,size)
# define NEW(type)           MALLOC(sizeof(type))
# define FREE(ptr)           gift_free(ptr)
# define REALLOC(ptr,size)   gift_realloc(ptr,size)
#else
# define MALLOC(size)        calloc(1,size)
#endif /* !TCG_LEAK_DETECT */

/*****************************************************************************/

/**
 * Wrapper for malloc that prevents malloc(0).
 */
void *gift_malloc (size_t size);

/**
 * Wrapper for calloc to prevent nmemb 0 or size 0.
 */
void *gift_calloc (size_t nmemb, size_t size);

/**
 * Dummy wrapper for free to just go along with this scheme.
 */
void gift_free (void *ptr);

/**
 * Realloc wrapper that prevents some broken implementations of realloc from
 * failing when ptr is NULL or size is 0.
 */
void *gift_realloc (void *ptr, size_t size);

/*****************************************************************************/

/**
 * Similar to strdup, except that it operates on raw memory.  I'm baffled why
 * such a function was not provided by the standard.
 */
void *gift_memdup (const void *ptr, size_t size);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __MEMORY_H */
