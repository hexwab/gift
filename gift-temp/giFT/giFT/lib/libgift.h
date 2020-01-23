/*
 * $Id: libgift.h,v 1.1 2003/03/28 07:32:42 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#ifndef __LIBGIFT_H
#define __LIBGIFT_H

/*****************************************************************************/

/**
 * @file libgift.h
 */

/*****************************************************************************/

#include "log.h"

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

/**
 * Initialize all libgiFT subsystems.  This should be a safe operation no
 * matter how much you decide to use libgiFT functionality.
 *
 * @param prog     Your programs identity, not argv[0].  For example, giFT
 *                 would use "giFT".
 * @param opt      Log options to use if the logging facility will be required.
 *                 It is recommended that you supply GLOG_STDERR for direct
 *                 output to stderr.
 * @param logfile  If the GLOG_FILE flag was set, this is the file that will
 *                 be written to.  Otherwise, it has no effect and it is
 *                 recommended that you supply NULL.
 */
int libgift_init (const char *prog, LogOptions opt, const char *logfile);

/**
 * Uninitialize any applicable data.  This is merely used for a graceful exit
 * and not strictly required.  At any rate, use it anyway :)
 */
int libgift_finish (void);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __LIBGIFT_H */
