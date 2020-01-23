#ifndef __LIST_LOCK_H
#define __LIST_LOCK_H

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

ListLock *list_lock_new           ();
void      list_lock_free          (ListLock *lock);
void      list_lock               (ListLock *lock);
void      list_unlock             (ListLock *lock);
void      list_lock_append        (ListLock *lock, void *data);
void      list_lock_prepend       (ListLock *lock, void *data);
void      list_lock_remove        (ListLock *lock, void *data);
#if 0
void      list_lock_insert_sorted (ListLock *lock, void *data);
#endif

/*****************************************************************************/

#endif /* __LIST_LOCK_H */
