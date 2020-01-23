/*
 * $Id: list_lock.h,v 1.6 2003/06/04 17:22:50 jasta Exp $
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

#ifndef __LIST_LOCK_H
#define __LIST_LOCK_H

/*****************************************************************************/

/**
 * @file list_lock.h
 *
 * @brief \todo
 *
 * \todo This.
 */

/*****************************************************************************/

typedef struct
{
	int   locked;

	List *lock_append;
	List *lock_prepend;
	List *lock_remove;
	List *lock_insert_sorted;

	List *list;
} ListLock;

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

ListLock *list_lock_new           ();
void      list_lock_free          (ListLock *lock);
void      list_lock               (ListLock *lock);
void      list_unlock             (ListLock *lock);
void      list_lock_append        (ListLock *lock, void *data);
void      list_lock_prepend       (ListLock *lock, void *data);
void      list_lock_remove        (ListLock *lock, void *data);
void      list_lock_insert_sorted (ListLock *lock, CompareFunc func, void *data);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __LIST_LOCK_H */
