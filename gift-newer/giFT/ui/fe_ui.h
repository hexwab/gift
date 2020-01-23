/*
 * fe_ui.h
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

#ifndef __FE_UI_H
#define __FE_UI_H

/**************************************************************************/

/* default dimensions */
#define DEF_WIDTH  700
#define DEF_HEIGHT 400

/**************************************************************************/

/* flags for packing boxes */
#define PACK_NONE    0x00
#define PACK_FILL    0x01
#define PACK_EXPAND  0x02

/* part helper function, part UI control :) */
#define PACK(box,widget,flags) gtk_box_pack_start (GTK_BOX (box), widget, \
							   ((flags) & PACK_FILL), \
							   ((flags) & PACK_EXPAND), 0)

#define PACK_END(box,widget,flags) gtk_box_pack_end (GTK_BOX (box), widget, \
								   ((flags) & PACK_FILL), \
								   ((flags) & PACK_EXPAND), 0)

/**************************************************************************/

/* used for various UI tasks */
#define SUPPORTED_FORMATS "Everything", "Audio", "Video", "Images", \
						  "Documents", "Software", NULL

/**************************************************************************/

int construct_main (FTApp *ft, char *title, char *cls);

/**************************************************************************/

#endif /* __FE_UI_H */
