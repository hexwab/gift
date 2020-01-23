/*
 * kzf2nodes.c - KaZuperNodes to giFT-FastTrack nodefile
 *                  Conversion Tool
 *
 * Copyright (C) 2004, Chris (Lance) Gilbert <Lance@thefrontnetworks.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{

	/* The Node's Hostname/IP */
	char *host; 

	/* The Node's Port */
	char *port; 	

	/* The Node's Country Code */
	char *country;

	/* The Node's "Klass" - Set To 1 For Supernode */
	int klass = 1; 

	/* The Node's Load - Set to 0 - Populated On Runtime */
	int load = 0; 

	/* Last Time The Node Was Seen - Set To 0 - Populated On Runtime */
	long int last_seen = 0;                                

	/* Sanity Checking For Our fscanf(s) */
	char *nextchar;

	/* Create File Descriptor for The Input File */
	FILE *infile;

	/* Create File Descriptor for The Output File */
	FILE *outfile;

	if(argc != 3)
	{
		printf("Usage: %s <input.kzf> <nodes>\n", argv[0]);
		return 1;
	}

	if( (outfile = fopen(argv[2], "w+" )) == NULL )
	{
		printf("Error Opening Output File\n");
		return 0;
	}
	fprintf(outfile,"# <host> <port> <klass> <load> <last_seen> <country>\n");

	if( (infile = fopen(argv[1], "r" )) == NULL )
	{
		printf("Error Opening Input File\n");
		return 0;
	}
	
	while (!feof(infile))
	{
		/* Hostname/IP Limit of 255 Characters */
		host = calloc(256,1);
        
		/* A Port Should Never be More Than 5 Characters */
		port = calloc(6,1);

		/* 2 Character Country Code */
		country = calloc(3,1);
			
		/* fscanf Sanity Checking - Only Gets One Character */
		nextchar = calloc(2,1);
			

		/* This Is Where Things Start To Get Ugly.... */

		fscanf(infile,"%*[^:]:%255[^:]%1s",host,nextchar);

		if(!strncmp(nextchar,":",1))
		{
			free(nextchar);
			nextchar = calloc(2,1);
			fscanf(infile,"%5[^|]%1s",port,nextchar);
		}
		else
		{
			free(nextchar);
			nextchar = calloc(2,1);
			fscanf(infile,"%*[^:]:%5[^|]%1s",port,nextchar);
		}
			
		if(!strncmp(nextchar,"|",1))
		{
			fscanf(infile,"%*[^|]|%*[^:]:%2[^|]%*[^\n]\n",country);
		}
		else
		{
			fscanf(infile,"%*[^|]|%*[^|]|%*[^:]:%2[^|]%*[^\n]\n", country);
		}
		
		if(!strcmp(country,""))
			strcpy(country,"XL");
				
		fprintf(outfile,"%s:%s %i %i %li %s\n",host,port,klass,load,last_seen, country);
		
		free(host);
		free(port);
		free(country);
		free(nextchar);
	}

	/* Close File Descriptor for The Input File */
	fclose(infile);
	
	/* Close File Descriptor for The Output File */
	fclose(outfile);
	
	return 0;
}
