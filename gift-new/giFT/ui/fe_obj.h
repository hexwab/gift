/*
 * fe_obj.h
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

#ifndef __FE_OBJ_H
#define __FE_OBJ_H

/**************************************************************************/

#define OBJ_NEW(type) obj_new (sizeof (type), type ## _Type)
/* what is the point of this? -Ross */
#define OBJ_FREE(obj) obj_free (obj)

#define OBJECT_DATA(obj) ((ObjectData *)(((char*)obj)+sizeof(unsigned long)))

/**************************************************************************/

typedef struct _object_data
{
	unsigned int type;
	GData *data_list;
#ifdef DEBUG
	unsigned long debug_tag;
#endif
} ObjectData;

/* base "class" for all event objects */

typedef struct
{
	unsigned long id;
	struct _object_data obj;
} Object;

#define OBJ_DATA(obj) (&((obj)->obj_data))

/**************************************************************************/

void *obj_new (size_t size, unsigned int type);
void obj_free (void *obj);

void obj_destroy_notify (void *obj);

void obj_set_data (ObjectData *obj_data, char *name, void *data);
void *obj_get_data (ObjectData *obj_data, char *name);
void obj_copy_data (ObjectData *obj_data, char *name, ObjectData *inc_data);

/**************************************************************************/

#endif /* __FE_OBJ_H */
