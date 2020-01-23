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

#ifndef __SL_META_H
#define __SL_META_H

#include "sl_soulseek.h"

/*****************************************************************************/

typedef enum
{
	META_ATTRIBUTE_BITRATE  = 0x00,
	META_ATTRIBUTE_LENGTH = 0x01
} SLMetaAttributeValues;

typedef struct
{
	uint32_t type;
	uint32_t value;
} SLMetaAttribute;

/*****************************************************************************/

#endif /* __SL_META_H */
