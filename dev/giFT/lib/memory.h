/*
 * memory.h
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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
#define CALLOC gift_calloc

#undef MALLOC
#define MALLOC(size) CALLOC(1,size)

#undef FREE
#define FREE gift_free

#undef REALLOC
#define REALLOC gift_realloc

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

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __MEMORY_H */
