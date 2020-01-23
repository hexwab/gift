/*
 * $Id: as_file.h,v 1.1 2004/09/16 18:24:46 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_FILE_H
#define __AS_FILE_H

/*****************************************************************************/

/* Returns TRUE if file exists */
as_bool as_file_exists (const char *path);

/* Returns pointer into path where filename begins */
char *as_get_filename (const char *path);

/*****************************************************************************/

#endif /* __AS_FILE_H */
