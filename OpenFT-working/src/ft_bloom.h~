/*
 * $Id$
 *
 * Copyright (C) 2001-2004 giFT project (gift.sourceforge.net)
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

#ifndef __FT_TOKENIZE_H
#define __FT_TOKENIZE_H

/*****************************************************************************/

/**
 * @file ft_tokenize.h
 *
 * @brief Tokenization of shares and queries for efficient lookup.
 *
 */

/*****************************************************************************/

/* Terminator value for order lists */
#define ORDER_END 0

/*
 * The value used to separate lists of tokens that should not be
 * treated as a single phrase.
 */
#define ORDER_SEP 1

/* Minimum value for real tokens */
#define ORDER_MIN 2

/* Maximum value for real tokens */
#define ORDER_MAX 255

/*****************************************************************************/

uint32_t *ft_tokenize_query (const char *string, uint8_t **order);

uint32_t *ft_tokenize_share (Share *file, uint8_t **order);

/*****************************************************************************/

#endif /* __FT_TOKENIZE_H */

