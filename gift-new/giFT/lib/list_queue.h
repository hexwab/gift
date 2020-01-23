#ifndef __LIST_QUEUE_H
#define __LIST_QUEUE_H

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

#endif /* __LIST_QUEUE_H */
