/*
 * obj.c
 *
 * C++ has possibly one good feature...but I say fuck the language if we can
 * do it in C
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

#include "gift-fe.h"

#include "obj.h"

/* for _Type definitions */
#include "search.h"
#include "transfer.h"

/**************************************************************************/

typedef void (*ObjDestroyFunc) (void *obj);

/*
 * stupid hash table for freeing GUI objects
 * jeez, this shit doesnt even look like valid syntax :)
 */
struct _free_table {
	unsigned int type;
	ObjDestroyFunc destroy_func;
}
free_table[] =
{
	{ Search_Type,   (ObjDestroyFunc) search_free },
	{ Transfer_Type, (ObjDestroyFunc) transfer_free },
	{ 0,           NULL },
};

#define OBJECT_DATA(obj) ((ObjectData *)(((char*)obj)+sizeof(unsigned long)))

/**************************************************************************/

void *obj_new (size_t size, unsigned int type)
{
	void *obj;

	/* size must be big enough to hold the type we wish to set */
	assert (size > sizeof (type));

	obj = malloc (size);
	assert (obj);

	memset (obj, 0, size);

	/* set the type */
	OBJECT_DATA (obj)->type = type;
	OBJECT_DATA (obj)->data_list = NULL;

	return obj;
}

void obj_free (void *obj)
{
	struct _free_table *ftbl_ptr;
	unsigned int type;
	int found = FALSE;

	assert (obj);

	type = OBJECT_DATA (obj)->type;

	/* search for the required type */
	for (ftbl_ptr = free_table; ftbl_ptr->destroy_func; ftbl_ptr++)
	{
		if (ftbl_ptr->type == type)
		{
			/* found...destroy it */
			(*ftbl_ptr->destroy_func) (obj);
			found = TRUE;
			return;
		}
	}

	/* clear the datalist */
	g_datalist_clear (&(OBJECT_DATA (obj)->data_list));

	if (!found)
		fprintf (stderr, "*** type '%u' for obj %p not found!\n", type, obj);
}

/**************************************************************************/

void obj_destroy_notify (void *obj)
{
	assert (obj);

	obj_free (obj);
}

/**************************************************************************/

void obj_set_data (ObjectData *obj_data, char *name, void *data)
{
	assert (obj_data);

	if (name)
		g_datalist_set_data (&obj_data->data_list, name, data);
}

void *obj_get_data (ObjectData *obj_data, char *name)
{
	assert (obj_data);

	return g_datalist_get_data (&obj_data->data_list, name);
}

void obj_copy_data (ObjectData *obj_data, char *name, ObjectData *inc_data)
{
	assert (obj_data);

	if (name)
	{
		obj_set_data (obj_data, name,
					  obj_get_data (inc_data, name));
	}
}
