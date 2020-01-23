/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001-2002 Göran Weinholt <weinholt@linux.nu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: gift.h,v 1.68 2002/11/06 11:46:44 chnix Exp $
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
int gift_emit(char *command, void *param);

/* this needs to be called upon startup/exit */
void gift_init(void);
void gift_cleanup(void);

int gift_write(ntree ** packet);

void gift_register(char *command, EventCallback, void *udata);
void gift_register_id(gift_id id, EventCallback, void *udata);

void gift_unregister(char *command, EventCallback, void *udata);
void gift_unregister_id(gift_id id);

#endif
