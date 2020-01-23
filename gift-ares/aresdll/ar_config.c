/*
 * $Id: ar_config.c,v 1.1 2005/12/18 16:43:38 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "aresdll.h"
#include "as_ares.h"
#include "ar_threading.h"
#include "ar_callback.h"

/*****************************************************************************/

static ASConfValId import_config_id (ARConfigKey key)
{
	switch (key)
	{
	case AR_CONF_PORT:         return AS_LISTEN_PORT;
	case AR_CONF_USERNAME:     return AS_USER_NAME;
	case AR_CONF_MAX_DOWNLODS: return AS_DOWNLOAD_MAX_ACTIVE;
	case AR_CONF_MAX_SOURCES:  return AS_DOWNLOAD_MAX_ACTIVE_SOURCES;
	case AR_CONF_MAX_UPLOADS:  return AS_UPLOAD_MAX_ACTIVE;
	}

	abort ();
}

/*****************************************************************************/

/*
 * Set integer config key to new value.
 */
as_bool ar_config_set_int (ARConfigKey key, as_int32 value)
{
	as_bool ret;

	if (!ar_events_pause ())
		return FALSE;

	ret = as_config_set_int (AS_CONF, import_config_id (key), value);

	ar_events_resume ();
	return ret;
}

/*
 * Set string config key to new value.
 */
as_bool ar_config_set_str (ARConfigKey key, const char *value)
{
	as_bool ret;

	if (!ar_events_pause ())
		return FALSE;

	ret = as_config_set_str (AS_CONF, import_config_id (key), value);

	ar_events_resume ();
	return ret;
}

/*
 * Get integer config key.
 */
as_int32 ar_config_get_int (ARConfigKey key)
{
	int ret;

	if (!ar_events_pause ())
		return FALSE;

	ret = as_config_get_int (AS_CONF, import_config_id (key));

	ar_events_resume ();
	return ret;
}

/*
 * Get string config key.
 */
const char *ar_config_get_str (ARConfigKey key)
{
	const char *ret;

	if (!ar_events_pause ())
		return FALSE;

	ret = as_config_get_str (AS_CONF, import_config_id (key));

	ar_events_resume ();
	return ret;
}

/*****************************************************************************/
