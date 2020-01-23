/*
 * $Id: ares_ip2port.c,v 1.5 2005/12/17 23:09:58 mkern Exp $
 *
 * Copyright (C) 2003 giFT-Ares project
 * http://developer.berlios.de/projects/gift-ares
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

#include <stdio.h>
#include <stdlib.h>

typedef unsigned int as_uint32;
typedef unsigned short as_uint16;
typedef unsigned char as_uint8;

#define FATAL_ERROR(x) { fprintf (stderr, "\nFATAL: %s\n", x); exit (1); }

/* ips may contain null bytes so we cannot rely on them for string
 * termination
 */
static as_uint16 hash_lowered_token (as_uint8 *str, int len)
{
	as_uint32 acc = 0;
	as_uint8 c;
	int b = 0;

	/* this is a very poor hash function :( */
	for (; len > 0; len--, str++)
	{
		c = tolower (*str);
		acc ^= c << (b * 8);
		b = (b + 1) & 3;
	}

	return (acc * 0x4f1bbcdc) >> 16;
}

static as_uint16 ip2port (as_uint32 ip)
{
	as_uint8 ip_str[4];
	as_uint8 tmp_str[4];
	as_uint16 ip_token;
	as_uint32 port;

	ip_str[0] = (ip >> 24) & 0xFF;
	ip_str[1] = (ip >> 16) & 0xFF;
	ip_str[2] = (ip >> 8)  & 0xFF;
	ip_str[3] = (ip)       & 0xFF;

	ip_token = hash_lowered_token (ip_str, 4);

	port = (((((as_uint16) ip_str[0]) * ip_str[0]) + ip_token) * 3);

	tmp_str[0] = port & 0xFF;
	tmp_str[1] = (port >> 8) & 0xFF;
	tmp_str[2] = 0xBE;
	tmp_str[3] = 0x04;

	port += hash_lowered_token (tmp_str, 4);
	port += ip_token + 0x12;
	port += 0x5907; /* token of "strano" */
	port -= (((as_uint16) ip_str[0] - 5) << 2) * 3;
	port += 0xCDF8; /* token of "robboso" */

	if (port < 1024)
		port += 1024;

	if (port == 36278)
		port++;

	return (port & 0xFFFF);
}


int main (int argc, char* argv[])
{
	as_uint32 ip;
	as_uint32 ipa, ipb, ipc, ipd; /* don't want to require socket lib here */
	as_uint16 port;


	if (argc != 3)
	{
		fprintf (stderr, "Usage: %s <format> <ip>\n", argv[0]);
		fprintf (stderr, "Format is one of:\n"
		                 "- 'signed' ip is a signed integer\n"
				 "- 'unsigned' ip is an unsigned integer\n"
				 "- 'dotted' ip is dotted decimal\n");
		exit (1);
	}

	if (!strcmp (argv[1], "signed"))
	{
		ip = (as_uint32) atoi (argv[2]);
		ipa = (ip >> 24) & 0xFF;
		ipb = (ip >> 16) & 0xFF;
		ipc = (ip >> 8) & 0xFF;
		ipd = (ip) & 0xFF;
	}
	else if (!strcmp (argv[1], "unsigned"))
	{
		ip = 0;
		sscanf(argv[2], "%u", &ip);
		ipa = (ip >> 24) & 0xFF;
		ipb = (ip >> 16) & 0xFF;
		ipc = (ip >> 8) & 0xFF;
		ipd = (ip) & 0xFF;	
	}
	else if (!strcmp (argv[1], "dotted"))
	{
		sscanf (argv[2], "%u.%u.%u.%u", &ipa, &ipb, &ipc, &ipd);
		ipa &= 0xFF; ipb &= 0xFF; ipc &= 0xFF; ipd &= 0xFF;
		ip = (ipa << 24) | (ipb << 16) | (ipc << 8)  | ipd;
	}
	else
	{
		FATAL_ERROR ("Invalid ip format");
	}


	port = ip2port (ip);

	printf ("%u.%u.%u.%u:%d\n", ipa, ipb, ipc, ipd, port);

}


