/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * giFTcurs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with giFTcurs; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: gift.h,v 1.71 2003/05/08 21:33:15 chnix Exp $
 */
#ifndef _GIFT_H
#define _GIFT_H

#include "protocol.h"

typedef unsigned int gift_id;
typedef int (*EventCallback) (void *param, void *udata);

/* Get the next available id. */
gift_id gift_new_id(void);
void gift_claimed_id(gift_id id);
void gift_release_id(gift_id id);
int gift_emit(const char *command, void *param);

/* this needs to be called upon startup/exit */
void gift_init(void);
void gift_cleanup(void);

int gift_write(ntree ** packet);

void gift_register(const char *command, EventCallback, void *udata);
void gift_register_id(gift_id id, EventCallback, void *udata);

void gift_unregister(const char *command, EventCallback, void *udata);
void gift_unregister_id(gift_id id);

#endif
