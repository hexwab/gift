/*
 * $Id: ft_event.h,v 1.3 2003/05/05 09:49:08 jasta Exp $
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

#ifndef __FT_EVENT_H
#define __FT_EVENT_H

/*****************************************************************************/

/**
 * @file ft_event.h
 *
 * @brief Assosicate a giFT-space interface event with a remotely executed
 * OpenFT operation.
 *
 * Bridge the cap between giFT-space and OpenFT-space event execution.  This
 * code is currently utilized by the search and browse mechanisms and is
 * well suited for any other operations which have a direct and logical
 * relationship between giFT and OpenFT space.
 */

/*****************************************************************************/

/**
 * Structure used to organize the association
 */
typedef struct
{
	unsigned int active : 8;           /**< current active state */
	IFEvent     *event;                /**< giFT event to associate with */
	IFEventID    id;                   /**< OpenFT identifier */

	char        *data_type;            /**< accessor string to unlink data */
	void        *data;                 /**< arbitrary user data associated with
										*   an OpenFT event */
} FTEvent;

/*****************************************************************************/

/**
 * Create a new FTEvent association.
 *
 * @param event     giFT event which we are to reply to.
 * @param hint
 *    Suggested identifier to start with, generally this should be the
 *    giFT-space id.  Any value is valid here, as it is not the true id.
 * @param data_type
 *    String-based type which FTEvent::data is to be represented by.  This
 *    allows for easy assumptions as to what type of event we are working
 *    with and makes a somewhat weak attempt at data security.
 * @param data      Arbitrary user-supplied data to track.
 */
FTEvent *ft_event_new (IFEvent *event, IFEventID hint,
                       char *data_type, void *data);

/**
 * Unallocate and remove the FTEvent association.
 */
void ft_event_free (FTEvent *ftev);

/**
 * Lookup the association.
 *
 * @param id OpenFT identifier returned from ::ft_event_new.
 */
FTEvent *ft_event_get (IFEventID id);

/**
 * Re-enable a previously disabled association.
 *
 * @see ft_event_disable
 */
void ft_event_enable (IFEvent *event);

/**
 * Disable a currently enabled association.
 *
 * @param event giFT event which the association replies to.
 */
void ft_event_disable (IFEvent *event);

/*****************************************************************************/

void *ft_event_data (FTEvent *ftev, char *data_type);
void *ft_event_id_data (IFEventID id, char *data_type);

/*****************************************************************************/

#endif /* __FT_EVENT_H */
