/*
 * Copyright (C) 2003 Arend van Beelen jr. (arend@auton.nl)
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

#ifndef __SL_STRING_H
#define __SL_STRING_H

#include "sl_soulseek.h"

/*****************************************************************************/

#define SL_STRING_DEFAULT_SIZE 512

/*****************************************************************************/

/**
 * sl_string structure
 */
typedef struct
{
	uint32_t  length;    // length of the string
	uint32_t  maxlength; // maximum length of the string
	char     *contents;  // the actual string
} sl_string;

/*****************************************************************************/

// allocates and returns a sl_string object
sl_string *sl_string_create();

// allocates and returns an empty sl_string object
sl_string *sl_string_create_empty();

// allocates and returns a sl_string object with the given size
sl_string *sl_string_create_with_size(uint32_t size);

// allocates and returns a sl_string object with a given contents
sl_string *sl_string_create_with_contents(char *contents);

// allocates and returns a sl_string object from a given block of data
sl_string *sl_string_create_from_data(char *data);

// creates a new string, copies the given string to it and returns the new string
sl_string *sl_string_copy(sl_string *string);

// returns the internal C string
char *sl_string_to_cstr(sl_string *string);

// creates a (dynamically allocated) data block and returns it
char *sl_string_to_data(sl_string *string);

// compare two slsk strings
int sl_string_cmp(sl_string *s1, sl_string *s2);

// destroys a created string
void sl_string_destroy(sl_string *string);

/*****************************************************************************/

#endif /* __SL_STRING_H */
