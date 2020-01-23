/*
 * $Id: http.c,v 1.2 2003/03/20 05:01:10 rossta Exp $
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

#include "gt_gnutella.h"

#include "http.h"

/*****************************************************************************/

static Dataset *http_protocols;

/*****************************************************************************/

HTTP_Protocol *http_protocol_new (Protocol *p)
{
	HTTP_Protocol *http;

	if (!(http = malloc (sizeof (HTTP_Protocol))))
		return NULL;

	http->p = p;

	return http;
}

void http_protocol_register (HTTP_Protocol *http)
{
	Protocol *p;

	if (!http_protocols)
		http_protocols = dataset_new (DATASET_LIST);

	p = http->p;

	assert (p);

	dataset_insert (&http_protocols, p, sizeof (Protocol), http, 0);
}

void http_protocol_unregister (HTTP_Protocol *http)
{
	dataset_remove (http_protocols, http->p, sizeof (Protocol));
}

HTTP_Protocol *http_protocol_get (Protocol *p)
{
	return dataset_lookup (http_protocols, p, sizeof (Protocol));
}
