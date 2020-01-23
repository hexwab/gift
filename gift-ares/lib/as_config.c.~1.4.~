/*
 * $Id: as_config.c,v 1.4 2004/11/20 03:02:15 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static ASConfVal *value_create (const ASConfVal *org)
{
	ASConfVal *val;

	if (!(val = malloc (sizeof (ASConfVal))))
		return FALSE;
	
	if (org)
	{
		val->id = org->id;
		val->name = gift_strdup (org->name);
		val->type = org->type;

		switch (org->type)
		{
		case AS_CONF_NOTYPE:
			val->data.s = NULL;
			break;
		case AS_CONF_INT:
			val->data.i = org->data.i;
			break;
		case AS_CONF_STR:
			val->data.s = gift_strdup (org->data.s);
			break;
		default:
			abort ();
		}

		val->change_cb = org->change_cb,
		val->udata = org->udata;
	}
	else
	{
		val->id = AS_CONF_VAL_ID_INVALID;
		val->name = NULL;
		val->type = AS_CONF_NOTYPE;
		val->data.s = NULL;
		val->change_cb = NULL,
		val->udata = NULL;
	}
	
	return val;
}

static void value_free (ASConfVal *val)
{
	if (!val)
		return;

	free (val->name);
	if (val->type == AS_CONF_STR)
		free (val->data.s);

	free (val);
}

static ASConfVal *value_get (ASConfig *config, ASConfValId id)
{
	/* Make sure value id is ok. */
	if (id < 0 || id > AS_CONF_VAL_ID_MAX)
	{
		AS_ERR_1 ("Value id %d out of range", id);
		assert (0);
		return NULL;
	}

	/* Make sure value id is set. */
	if (config->values[id] == NULL)
	{
		AS_ERR_1 ("Value with id %d not added", id);
		assert (0);
		return NULL;
	}

	return config->values[id];
}


/*****************************************************************************/

/* Allocate and init config object. */
ASConfig *as_config_create ()
{
	ASConfig *config;
	int i;

	if (!(config = malloc (sizeof (ASConfig))))
		return NULL;

	for (i = 0; i <= AS_CONF_VAL_ID_MAX; i++)
		config->values[i] = NULL;

	return config;
}

/* Free config object. */
void as_config_free (ASConfig *config)
{
	int i;

	if (!config)
		return;

	for (i = 0; i <= AS_CONF_VAL_ID_MAX; i++)
		value_free (config->values[i]);

	free (config);
}

/*****************************************************************************/

/* Add nvals values to config system. Each value must be added before it can
 * be used. If a value with the same id is already present this will fail.
 */
as_bool as_config_add_values (ASConfig *config, const ASConfVal *vals,
                              int nvals)
{
	int i;

	/* Add all values */
	for (i = 0; i < nvals; i++)
	{
		/* Make sure value id is ok. */
		if (vals[i].id < 0 || vals[i].id > AS_CONF_VAL_ID_MAX)
		{
			AS_ERR_1 ("Value id %d out of range", vals[i].id);
			assert (0);
			return FALSE;
		}

		/* Make sure value id is not already used. */
		if (config->values[vals[i].id] != NULL)
		{
			AS_ERR_1 ("Value with id %d already present", vals[i].id);
			assert (0);
			return FALSE;
		}

		/* Add value. */
		if (!(config->values[vals[i].id] = value_create (&vals[i])))
		{
			AS_ERR ("Insufficient memory");
			return FALSE;	
		}
	}

	return TRUE;
}

/* Remove value. */
as_bool as_config_remove_value (ASConfig *config, ASConfValId id)
{
	ASConfVal *val;

	if (!(val = value_get (config, id)))
		return FALSE;

	value_free (val);
	config->values[id] = NULL;

	return TRUE;	
}

/*****************************************************************************/

/* Set integer value. */
as_bool as_config_set_int (ASConfig *config, ASConfValId id, int integer)
{
	ASConfVal *val, *new_val;
	as_bool ret = TRUE;

	if (!(val = value_get (config, id)))
		return FALSE;

	if (val->type != AS_CONF_INT)
	{
		assert (val->type == AS_CONF_INT);
		return FALSE;
	}

	/* If value didn't change there is nothing to do. */
	if (val->data.i == integer)
		return TRUE;

	/* Create new val. */
	if (!(new_val = value_create (val)))
		return FALSE;
	
	new_val->data.i = integer;

	/* Raise callback */
	if (val->change_cb)
		ret = val->change_cb (val, new_val, val->udata);

	/* If callback refused change return now. */
	if (!ret)
	{
		value_free (new_val);
		return FALSE;
	}

	/* Switch values */
	value_free (val);
	config->values[id] = new_val;

	return TRUE;
}

/* Set string value. */
as_bool as_config_set_str (ASConfig *config, ASConfValId id, const char *str)
{
	ASConfVal *val, *new_val;
	as_bool ret = TRUE;

	if (!(val = value_get (config, id)))
		return FALSE;

	if (val->type != AS_CONF_STR)
	{
		assert (val->type == AS_CONF_STR);
		return FALSE;
	}

	/* If value didn't change there is nothing to do. */
	if (gift_strcmp (val->data.s, str) == 0)
		return TRUE;

	/* Create new val. */
	if (!(new_val = value_create (val)))
		return FALSE;
	
	free (new_val->data.s);
	new_val->data.s = gift_strdup (str);

	/* Raise callback */
	if (val->change_cb)
		ret = val->change_cb (val, new_val, val->udata);

	/* If callback refused change return now. */
	if (!ret)
	{
		value_free (new_val);
		return FALSE;
	}

	/* Switch values */
	value_free (val);
	config->values[id] = new_val;

	return TRUE;
}

/* Set change callback. */
as_bool as_config_set_callback (ASConfig *config, ASConfValId id,
                                ASConfigValChangeCb callback, void *udata)
{
	ASConfVal *val;

	if (!(val = value_get (config, id)))
		return FALSE;

	val->change_cb = callback;
	val->udata = udata;

	return TRUE;
}

/*****************************************************************************/

ASConfValType as_config_get_type (ASConfig *config, ASConfValId id)
{
	ASConfVal *val;

	if (!(val = value_get (config, id)))
		return AS_CONF_NOTYPE;

	return val->type;
}

const char *as_config_get_name (ASConfig *config, ASConfValId id)
{
	ASConfVal *val;

	if (!(val = value_get (config, id)))
		return NULL;

	return val->name;
}

int as_config_get_int (ASConfig *config, ASConfValId id)
{
	ASConfVal *val;

	if (!(val = value_get (config, id)))
		return 0;

	if (val->type != AS_CONF_INT)
	{
		assert (val->type == AS_CONF_INT);
		return 0;
	}

	return val->data.i;
}

const char *as_config_get_str (ASConfig *config, ASConfValId id)
{
	ASConfVal *val;

	if (!(val = value_get (config, id)))
		return NULL;

	if (val->type != AS_CONF_STR)
	{
		assert (val->type == AS_CONF_STR);
		return NULL;
	}

	return val->data.s;
}

/*****************************************************************************/

/* These functions use giFT's config system to read and write files. */

#ifdef GIFT_PLUGIN

/* Saves all values for which value->name != NULL in file. */
as_bool as_config_save (ASConfig *config, const char *file)
{
	assert (0);
}

/* Loads all vaulues for which value->name != NULL from file. Triggers change
 * callback if set.
 */
as_bool as_config_load (ASConfig *config, const char *file)
{
	assert (0);
}

#endif

/*****************************************************************************/

