/* giFT OpenNap
 * Copyright (C) 2003 Tilman Sauerbeck <tilman@code-monkey.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __OPN_SEARCH_H
#define __OPN_SEARCH_H

typedef struct {
	IFEvent *event;
	char query[128];
	char exclude[128];
	uint32_t ref;
} OpnSearch;

OpnSearch *opn_search_new();
uint32_t opn_search_ref(OpnSearch *search);
uint32_t opn_search_unref(OpnSearch *search);
void opn_search_free(OpnSearch *search);

OpnSearch *opn_search_find(char *query);

BOOL gift_cb_search(Protocol *p, IFEvent *event, char *query, char *exclude,
                    char *realm, Dataset *meta);

#endif

