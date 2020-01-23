/*
 * $Id: template.h,v 1.1 2003/03/28 07:32:43 jasta Exp $
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

#ifndef __TEMPLATE_H
#define __TEMPLATE_H

/*****************************************************************************/

/**
 * @file template.h
 *
 * @brief Templating engine for the HTML-based portions of the code
 */

/*****************************************************************************/

/**
 * Template object which is used for processing template files.  Template
 * files have special syntax with meaning only to this subsystem.
 */
typedef struct
{
	/**
	 * @name Input Sources
	 */
	char    *file;                     /**< Filename to read from */
	char    *buf;                      /**< Static buffer of all data if
	                                    *   you wish to hardcode the template
	                                    *   text */

	/**
	 * @name Substitution Handling
	 */
	Dataset *data;                     /**< Holds substitution data */
} Template;

/*****************************************************************************/

#endif /* __TEMPLATE_H */
