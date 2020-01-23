/*
 * $Id: ar_threading.h,v 1.1 2005/12/18 16:43:38 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AR_THREADING_H
#define __AR_THREADING_H

/*****************************************************************************/

#include "aresdll.h"

/*****************************************************************************/

as_bool ar_start_event_thread ();

as_bool ar_stop_event_thread ();

/*****************************************************************************/

/*
 * Pause event system from different thread so ares lib can be accessed
 * safely.
 */
as_bool ar_events_pause ();

/*
 * Resume pause event system.
 */
as_bool ar_events_resume ();

/*****************************************************************************/

#endif /* __AR_THREADING_H */
