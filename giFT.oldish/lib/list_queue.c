#include "gift.h"
#include "list.h"

#include "list_queue.h"

/*****************************************************************************/

ListQueue *list_queue_new (List *list)
{
	ListQueue *queue;

	if (!(queue = malloc (sizeof (ListQueue))))
		return NULL;

	memset (queue, 0, sizeof (ListQueue));

	queue->list = list;

	return queue;
}

void list_queue_free (ListQueue *queue)
{
	if (!queue)
		return;

	list_free (queue->list);
	free (queue);
}

/*****************************************************************************/

/* my $data = shift @queue; */
void *list_queue_shift (ListQueue *queue)
{
	List *link;
	void *data;

	if (!queue)
		return NULL;

	if (!(link = list_nth (queue->list, 0)))
		return NULL;

	data = link->data;

	queue->list = list_remove_link (queue->list, link);

	return data;
}

/* unshift @queue, $data; */
void list_queue_unshift (ListQueue *queue, void *data)
{
	if (!queue)
		return;

	queue->list = list_prepend (queue->list, data);
}

/*****************************************************************************/

/* push @queue, $data; */
void list_queue_push (ListQueue *queue, void *data)
{
	if (!queue)
		return;

	queue->list = list_append (queue->list, data);
}

/* my $data = pop @queue; */
void *list_queue_pop (ListQueue *queue)
{
	List *tail;
	void *data;

	if (!queue)
		return NULL;

	if (!(tail = list_last (queue->list)))
		return NULL;

	data = tail->data;

	queue->list = list_remove_link (queue->list, tail);

	return data;
}

/*****************************************************************************/

#if 0
int main ()
{
	ListQueue *queue;

	queue = list_queue_new (NULL);

	list_queue_push (queue, "1");
	list_queue_push (queue, "2");
	list_queue_push (queue, "3");

	printf ("%s\n", (char *) list_queue_shift (queue));

	list_queue_push (queue, "4");

	printf ("%s\n", (char *) list_queue_shift (queue));

	list_queue_push (queue, "5");

	while (queue && queue->list)
		printf ("%s\n", (char *) list_queue_shift (queue));

	list_queue_push (queue, "6");

	printf ("%s\n", (char *) list_queue_shift (queue));

	list_queue_free (queue);

	return 0;
}
#endif
