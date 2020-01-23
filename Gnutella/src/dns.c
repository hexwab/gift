/*
 * $Id: dns.c,v 1.2 2003/12/26 12:02:20 mkern Exp $
 *
 * Copyright (C) 2003 giFT project (gift.sourceforge.net)
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
#include "dns.h"

/*****************************************************************************/

struct hostent *gt_dns_lookup (const char *name)
{
	return gethostbyname (name);
}

int gt_dns_get_errno (void)
{
#ifdef HAVE_HSTRERROR /* assume h_errno present if hstrerror() is */
	return h_errno;
#else
#ifdef WIN32
	return WSAGetLastError ();
#endif /* WIN32 */
	return 0;
#endif
}

void gt_dns_set_errno (int error_code)
{
#ifdef HAVE_HSTRERROR
	h_errno = 0;
#else
#ifdef WIN32
	WSASetLastError (error_code);
#endif /* WIN32 */
#endif
}

const char *gt_dns_strerror (int error_code)
{
#ifdef HAVE_HSTRERROR
	return hstrerror (error_code);
#else
	return "Host name lookup failure";
#endif
}
