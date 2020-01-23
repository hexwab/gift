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

#ifndef __OPN_PROTOCOL_HANDLERS_H
#define __OPN_PROTOCOL_HANDLERS_H

OPN_HANDLER(error);

OPN_HANDLER(login_error);
OPN_HANDLER(login_ack);

OPN_HANDLER(search_result);
OPN_HANDLER(search_finished);

OPN_HANDLER(download_ack);

OPN_HANDLER(stats);

OPN_HANDLER(ping);

#endif

