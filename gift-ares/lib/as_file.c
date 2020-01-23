/*
 * $Id: as_file.c,v 1.1 2004/09/16 18:24:46 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* [insert obligatory rant here] */
#ifdef WIN32
#define DIRSEP "/\\"
#else
#define DIRSEP "/"
#endif

/*****************************************************************************/

/* Returns TRUE if file exists */
as_bool as_file_exists (const char *path)
{
	struct stat st;

	if (stat (path, &st) == 0 && S_ISREG (st.st_mode))
		return TRUE;

	return FALSE;	
}

/* Returns pointer into path where filename begins */
char *as_get_filename (const char *path)
{
	char *name = (char *) path;
	size_t n;

	/* gah, strrcspn() doesn't exist, so we have to cobble
	 * something else together */
	while (name[n = strcspn (name, DIRSEP)])
		name += n + 1;

	return name;
}

/*****************************************************************************/
