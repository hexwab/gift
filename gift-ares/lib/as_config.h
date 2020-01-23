/*
 * $Id: as_config.h,v 1.1 2004/11/06 18:08:18 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_CONFIG_H_
#define __AS_CONFIG_H_

/*****************************************************************************/

/* All values are accessed using this integer id. */
typedef int ASConfValId;
#define AS_CONF_VAL_ID_MAX 64
#define AS_CONF_VAL_ID_INVALID -1

/* config value types */
typedef enum
{
	AS_CONF_NOTYPE = 0,
	AS_CONF_INT    = 1,
	AS_CONF_STR    = 2
} ASConfValType;

typedef struct as_conf_val_t ASConfVal;

/* Called before a value changes. If FALSE is returned the value is not
 * changed.
 */
typedef as_bool (*ASConfigValChangeCb) (const ASConfVal *old_val,
                                        const ASConfVal *new_val,
                                        void *udata);

/* The struct representing a config value. */
struct as_conf_val_t
{
	ASConfValId id;
	char *name;      /* human readable name, may be NULL */

	ASConfValType type;

	/* Actual value data depending on type. */
	union
	{
		int i;
		char *s;
	} data;

	/* Callback and it's arbitrary user data. */
	ASConfigValChangeCb change_cb;
	void *udata;
};

/* The config object. */
typedef struct
{
	/* Array of values keyed by value->id. */
	ASConfVal *values[AS_CONF_VAL_ID_MAX + 1]; 

} ASConfig;

/*****************************************************************************/

/* Allocate and init config object. */
ASConfig *as_config_create ();

/* Free config object. */
void as_config_free (ASConfig *config);

/*****************************************************************************/

/* Add nvals values to config system. Each value must be added before it can
 * be used. If a value with the same id is already present this will fail.
 */
as_bool as_config_add_values (ASConfig *config, const ASConfVal *vals,
                              int nvals);

/* Remove value. */
as_bool as_config_remove_value (ASConfig *config, ASConfValId id);

/*****************************************************************************/

/* Set integer value. */
as_bool as_config_set_int (ASConfig *config, ASConfValId id, int integer);

/* Set string value. */
as_bool as_config_set_str (ASConfig *config, ASConfValId id, const char *str);

/* Set change callback. */
as_bool as_config_set_callback (ASConfig *config, ASConfValId id,
                                ASConfigValChangeCb callback, void *udata);

/*****************************************************************************/

ASConfValType as_config_get_type (ASConfig *config, ASConfValId id);

const char *as_config_get_name (ASConfig *config, ASConfValId id);

int as_config_get_int (ASConfig *config, ASConfValId id);

const char *as_config_get_str (ASConfig *config, ASConfValId id);

/*****************************************************************************/

/* These functions use giFT's config system to read and write files. */

#ifdef GIFT_PLUGIN

/* Saves all values for which value->name != NULL in file. */
as_bool as_config_save (ASConfig *config, const char *file);

/* Loads all vaulues for which value->name != NULL from file. Triggers change
 * callback if set.
 */
as_bool as_config_load (ASConfig *config, const char *file);

#endif

/*****************************************************************************/

#endif /* __AS_CONFIG_H_ */
