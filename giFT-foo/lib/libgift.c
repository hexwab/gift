/*
 * $Id: libgift.c,v 1.2 2003/05/25 23:03:23 jasta Exp $
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

#include "libgift.h"

#include "platform.h"
#include "log.h"
#include "event.h"

/*****************************************************************************/

int libgift_init (const char *prog, LogOptions opt, const char *logfile)
{
	/*
	 * Initialize all platform specific (Windows, UNIX, etc) systems that
	 * giFT will use.  This is required before you can use the configuration
	 * path systems and as such most applications will need it.
	 */
	platform_init ();

	/*
	 * Setup the logging facilities with the subset arguments supplied by the
	 * caller.  If you wish to use more complicated log_init options you must
	 * use libgift_initfull (which is not currently available).
	 */
	log_init (opt, (char *)prog, 0, 0, (char *)logfile);

	/*
	 * Required before any inputs can be added or the event_loop can be
	 * entered.
	 */
	event_init ();

	/*
	 * Placebo return value.
	 */
	return TRUE;
}

int libgift_finish (void)
{
	/*
	 * Ensure that the event loop will be abandoned after the next iteration,
	 * just in case this isn't already the case.
	 */
	event_quit (2 /* SIGINT */);

	/*
	 * Prevent an fd leak.
	 */
	log_cleanup ();

	/*
	 * Mostly just terminates any child processes that may have been spawned.
	 */
	platform_cleanup ();

	/*
	 * Again, this return value is not meaningful, but it appears to be.
	 */
	return TRUE;
}
