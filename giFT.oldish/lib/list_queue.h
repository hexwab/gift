#ifndef __LIST_QUEUE_H
#define __LIST_QUEUE_H

/*****************************************************************************/

/**
 * @file list_queue.h
 *
 * @brief \todo
 *
 * \todo This.
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

typedef struct
{
	List *list;
} ListQueue;

/*****************************************************************************/

ListQueue *list_queue_new  (List *list);
void       list_queue_free (ListQueue *queue);

void *list_queue_shift   (ListQueue *queue);
void  list_queue_unshift (ListQueue *queue, void *data);
void  list_queue_push    (ListQueue *queue, void *data);
void *list_queue_pop     (ListQueue *queue);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __LIST_QUEUE_H */
