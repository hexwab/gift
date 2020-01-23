/*
 * $Id: main.h,v 1.6 2004/09/17 11:40:34 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __MAIN_H
#define __MAIN_H

/*****************************************************************************/

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "as_ares.h"

#include "cmd.h"
#include "testing.h"

/*****************************************************************************/

int parse_argv(char *cmdline, int *argc, char ***argv);

/*****************************************************************************/

#endif /* __MAIN_H */
