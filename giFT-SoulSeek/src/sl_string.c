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

#include <assert.h>
#include <string.h>

#include "sl_soulseek.h"
#include "sl_string.h"

/*****************************************************************************/

// allocates and returns a sl_string object
sl_string *sl_string_create()
{
	sl_string *new_string = (sl_string *) MALLOC(sizeof(sl_string));

	new_string->length = 0;
	new_string->maxlength = SL_STRING_DEFAULT_SIZE;
	new_string->contents = (char *) MALLOC(SL_STRING_DEFAULT_SIZE);

	return new_string;
}

// allocates and returns an empty sl_string object
sl_string *sl_string_create_empty()
{
	sl_string *new_string = (sl_string *) MALLOC(sizeof(sl_string));

	new_string->length = 0;
	new_string->maxlength = 0;
	new_string->contents = NULL;

	return new_string;
}

// allocates and returns a sl_string object with the given size
sl_string *sl_string_create_with_size(uint32_t size)
{
	sl_string *new_string = (sl_string *) MALLOC(sizeof(sl_string));

	new_string->length = 0;
	new_string->maxlength = size;
	new_string->contents = (char *) MALLOC(size);

	return new_string;
}

// allocates and returns a sl_string object with a given contents
sl_string *sl_string_create_with_contents(char *contents)
{
	assert(contents != NULL);

	sl_string *new_string = (sl_string *) MALLOC(sizeof(sl_string));

	new_string->length = strlen(contents);
	new_string->maxlength = new_string->length;
	new_string->contents = (char *) MALLOC(new_string->length + 1);

	memcpy(new_string->contents, contents, new_string->length + 1);

	return new_string;
}

// allocates and returns a sl_string object from a given block of data
sl_string *sl_string_create_from_data(char *data)
{
	assert(data != NULL);

	sl_string *new_string = (sl_string *) MALLOC(sizeof(sl_string));

	memcpy(&new_string->length, data, sizeof(uint32_t));
	new_string->maxlength = new_string->length;
	new_string->contents = (char *) MALLOC(new_string->length + 1);
	
	memcpy(new_string->contents, data + sizeof(uint32_t), new_string->length);
	new_string->contents[new_string->length] = '\0';

	return new_string;
}

// creates a new string, copies the given string to it and returns the new string
sl_string *sl_string_copy(sl_string *string)
{
	assert(string != NULL);

	sl_string *new_string = (sl_string *) MALLOC(sizeof(sl_string));

	new_string->length = string->length;
	new_string->maxlength = string->maxlength;

	if(string->contents == NULL)
	{
		new_string->contents = NULL;
	}
	else
	{
		new_string->contents = (char *) MALLOC(string->maxlength + 1);
		memcpy(new_string->contents, string->contents, string->length + 1);
	}

	return new_string;
}

// returns the internal C string
char *sl_string_to_cstr(sl_string *string)
{
	assert(string != NULL);

	return string->contents;
}

// creates a (dynamically allocated) data block and returns it
char *sl_string_to_data(sl_string *string)
{
	assert(string != NULL);

	char *new_data = (char *) MALLOC(string->length + 4);

	(uint32_t) *new_data = string->length;

	memcpy(new_data + 4, string->contents, string->length);

	return new_data;
}

// compare two slsk strings
int sl_string_cmp(sl_string *s1, sl_string *s2)
{
	assert(s1 != NULL);
	assert(s2 != NULL);

	if (s1->length > s2->length)
		return -1;
	
	if (s1->length < s2->length)
		return 1;

	return memcmp(s1->contents, s2->contents, s1->length);
}

// destroys a created string
void sl_string_destroy(sl_string *string)
{
	assert(string != NULL);

	if(string->contents != NULL)
		free(string->contents);
		
	free(string);
}

/*****************************************************************************/
