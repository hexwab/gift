#include "ft_openft.h"

#include "ft_event.h"

/*****************************************************************************/

static Dataset *ft_events = NULL;

/*****************************************************************************/

static IFEventID ev_id (IFEventID hint)
{
	while (hint == 0 || dataset_lookup (ft_events, &hint, sizeof (hint)))
		hint++;

	return hint;
}

FTEvent *ft_event_new (IFEvent *event, IFEventID hint,
                       char *data_type, void *data)
{
	FTEvent   *ftev;
	IFEventID  id;

	if ((!data_type && data) || !event)
		return NULL;

	if (!(id = ev_id (hint)))
		return NULL;

	if (!(ftev = MALLOC (sizeof (FTEvent))))
		return NULL;

	ftev->active    = TRUE;
	ftev->event     = event;
	ftev->id        = id;
	ftev->data_type = STRDUP (data_type);
	ftev->data      = data;

	dataset_insert (&ft_events, &id, sizeof (id), ftev, 0);

	return ftev;
}

void ft_event_free (FTEvent *ftev)
{
	if (!ftev)
		return;

	dataset_remove (ft_events, &ftev->id, sizeof (ftev->id));

	free (ftev->data_type);
	free (ftev);
}

/*****************************************************************************/

FTEvent *ft_event_get (IFEventID id)
{
	return dataset_lookup (ft_events, &id, sizeof (id));
}

static int by_event (Dataset *d, DatasetNode *node, IFEvent *event)
{
	FTEvent *ftev = node->value;

	return (ftev->event == event);
}

static FTEvent *get_by_event (IFEvent *event)
{
	return dataset_find (ft_events, DATASET_FOREACH (by_event), event);
}

/*****************************************************************************/

static void set_active (IFEvent *event, int active)
{
	FTEvent *ftev;

	if (!(ftev = get_by_event (event)))
		return;

	ftev->active = active;
}

void ft_event_enable (IFEvent *event)
{
	set_active (event, TRUE);
}

void ft_event_disable (IFEvent *event)
{
	set_active (event, FALSE);
}

/*****************************************************************************/

void *ft_event_data (FTEvent *ftev, char *data_type)
{
	if (!ftev || !data_type)
		return NULL;

	if (strcmp (ftev->data_type, data_type))
		return NULL;

	return ftev->data;
}

void *ft_event_id_data (IFEventID id, char *data_type)
{
	FTEvent *ftev;

	if (!(ftev = ft_event_get (id)) || !ftev->active)
		return NULL;

	return ft_event_data (ftev, data_type);
}
