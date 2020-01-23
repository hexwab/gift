/*
 * $Id: ar_misc.h,v 1.1 2005/12/18 16:43:38 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AR_MISC_H
#define __AR_MISC_H

/*****************************************************************************/

#include "aresdll.h"

/*****************************************************************************/

/* ASNetInfo stat callback which forwards state to user application */
void ar_stats_callback (ASNetInfo *info);

/*****************************************************************************/

/* Set meta data array from lib object */
void ar_export_meta (ARMetaTag meta[AR_MAX_META_TAGS], ASMeta *libmeta);

/* Create meta data object from external array */
ASMeta *ar_import_meta (ARMetaTag meta[AR_MAX_META_TAGS]);

/*****************************************************************************/

#endif /* __AR_MISC_H */
