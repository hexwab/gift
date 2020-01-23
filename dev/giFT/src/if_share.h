/*
 * if_share.h
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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

#ifndef __IF_SHARE_H
#define __IF_SHARE_H

/*****************************************************************************/

IFEvent  *if_share_new     (Connection *c, IFEventID requested);
void      if_share_file    (IFEvent *event, FileShare *file);
void      if_share_action  (IFEvent *event, char *action, char *status);
void      if_share_finish  (IFEvent *event);

/*****************************************************************************/

#endif /* __IF_SHARE_H */
