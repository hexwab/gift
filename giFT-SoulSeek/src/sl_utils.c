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

#include "sl_soulseek.h"
#include "sl_utils.h"

//#include <asm/byteorder.h>

/*****************************************************************************/

uint32_t sl_swap_byte_order(uint32_t integer)
{
	uint8_t byte0;
	uint8_t byte1;
	uint8_t byte2;
	uint8_t byte3;

	byte0 = (integer & 0x000000ff);
	byte1 = (integer & 0x0000ff00) >> 8;
	byte2 = (integer & 0x00ff0000) >> 16;
	byte3 = (integer & 0xff000000) >> 24;

	return (byte0 << 24) | (byte1 << 16) | (byte2 << 8) | (byte3);
}

void sl_switch_forward_slashes(sl_string *string)
{
	int i;

	for(i = 0; i < string->length; i++)
	{
		if(string->contents[i] == '\\')
			string->contents[i] = '/';
	}
}

/*****************************************************************************/
