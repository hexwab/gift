/*
 * $Id: ar_callback.h,v 1.1 2005/12/18 16:43:38 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AR_CALLBACK_H
#define __AR_CALLBACK_H

/*****************************************************************************/

#include "aresdll.h"

/*****************************************************************************/

as_bool ar_create_callback_system (ARCallback callback);

as_bool ar_destroy_callback_system ();

/*****************************************************************************/

/* Call user in his own thread. */
as_bool ar_raise_callback (ARCallbackCode code, void *param1, void *param2);

/* Returns TRUE if we are in a callback */
as_bool ar_callback_active ();

/*****************************************************************************/

#endif /* __AR_CALLBACK_H */
