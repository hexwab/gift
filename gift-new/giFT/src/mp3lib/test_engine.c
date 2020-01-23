//  mp3info - reads mp3 headers/tags information
//  Copyright (C) 2002  Shachar Raindel
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Library General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Library General Public License for more details.
//
//  You should have received a copy of the GNU Library General Public
//  License along with this library; if not, write to the Free
//  Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
//  MA 02111-1307, USA
//
#include <sys/stat.h>
#include <stdio.h>
#ifdef __WIN32__
#include <windows.h>
#include <io.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "mp3info.h"

char *file_to_check = "/mnt/c/milk.MP3";
char *file_name = "milk.MP3";
char *file_path = "/mnt/c/";
char *file_extension = "MP3";

extern "C"  int read_mp3_data( long unsigned size , mp3_file_info *current , unsigned long int max_size,unsigned long int min_size ,unsigned long int max_sec ,unsigned long int min_sec);

extern char *genres_list[];
void main()
{
	mp3_file_info the_file_info;
	struct stat stat_buf;
	memset(&the_file_info , 0 , sizeof(mp3_file_info));

//	the_file_info.struct_size = sizeof(mp3_file_info);
//	the_file_info.version = MP3LIST_H_VERSION;
	the_file_info.file_name = file_name;
    the_file_info.file_path = file_path;
	the_file_info.file_extantion = file_extension;
	the_file_info.function_list = NULL;


  //	if(access(file_to_check , 2) != 0)
    //	{
       //		return ;
     //	}

	stat(file_to_check , &stat_buf);

	read_mp3_data(stat_buf.st_size , &the_file_info , 0 , 0 ,0 ,0);
	printf( "struct returned:\n"
	"file name - %s\n"
	"title - %s\n"
	"artist - %s\n"
	"album - %s\n"
	"comment - %s\n"
	"year - %s\n"
	"genre - %d\n"
	"genre string - %s\n",the_file_info.file_name,the_file_info.title,the_file_info.artist,the_file_info.album,the_file_info.comment,
	the_file_info.year,the_file_info.genre,the_file_info.genre==GENRE_MAGIC?the_file_info.genre_string:genres_list[the_file_info.genre]);
	
	printf( "v2 info:\n"
	"title - %s\n"
	"artist - %s\n"
	"album - %s\n"
	"comment - %s\n"
	"year - %s\n"
	"track number - %d\n"
	"genre string - %s\n",the_file_info.v2_title,the_file_info.v2_artist,the_file_info.v2_album,the_file_info.v2_comment,
	the_file_info.v2_year,the_file_info.v2_track_number,the_file_info.v2_genre_string);
	
	
	
	
	
	if(the_file_info.function_list)
	{
		finialization_functions_list *func , *func2;
		for(func = the_file_info.function_list ; func ; func = func2)
		{
			func2 = func->next;// the function should also free its call structure, so this should be done before calling it.
			if(func->function)
			{
				func->function(func , &the_file_info);
			}
		}
	}



}



