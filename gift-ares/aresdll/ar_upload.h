/*
 * $Id: ar_upload.h,v 1.1 2005/12/18 16:43:38 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AR_UPLOAD_H
#define __AR_UPLOAD_H

/*****************************************************************************/

#include "aresdll.h"

/*****************************************************************************/

/* Set state and progress callbacks with upload manager */
void ar_upload_add_callbacks ();

/* Remove state and progress callbacks from upload manager */
void ar_upload_remove_callbacks ();

/*****************************************************************************/

#endif /* __AR_UPLOAD_H */
