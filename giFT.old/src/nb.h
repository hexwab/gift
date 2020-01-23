/*
 * nb.h
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

#ifndef __NB_H
#define __NB_H

/**************************************************************************/

typedef struct {
   int type;
   int nb_type;

   char *data;
   int datalen;
   int len;

   int term;
   int flag;
} NBRead;

/**************************************************************************/

/* prototypes */
NBRead *nb_active      (int type);
void    nb_terminate   (NBRead *nb);
void    nb_add         (NBRead *nb, char c);
int     nb_read        (NBRead *nb, int source, size_t count, char *term);
void    nb_free        (NBRead *nb);
void    nb_finish      (NBRead *nb);
void    nb_destroy     (int type);
void    nb_destroy_all ();
/* !prototypes */

/**************************************************************************/

#endif /* __NB_H */
