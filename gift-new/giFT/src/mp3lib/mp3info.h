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
#ifndef MP3LISTH
#define MP3LISTH
#include <stdio.h>

#ifndef __WIN32__
#define __stdcall 
#endif

/*-----------------22/12/01 19:40-------------------
 * TODO: document this file
 * --------------------------------------------------*/

#define MP3INFO_H_VERSION 0xAE010000
						  /*MjMiBuild*/
/*-----------------09/12/01 15:12-------------------
 * Major(1 byte).Minor(1 byte).Build(2 bytes)
 * --------------------------------------------------*/


typedef struct finialization_functions_list_tag {
	void *private_data;
	void (*function)(struct finialization_functions_list_tag * , struct mp3_file_info_type * );
	struct finialization_functions_list_tag *next;
} finialization_functions_list;

typedef struct mp3_file_info_type {
//    size_t struct_size;
//    unsigned long version;
//    struct mp3_file_info_type *previous;
    char *file_path;
    char *file_name;
    char *file_extantion;
	
    bool id3v1found;
    char *title;
    char *artist;
    char *album;
    char *comment;
    char *year;
       unsigned char genre;
    char *genre_string;
    
    bool id3v2found;
    char *v2_title;
    char *v2_artist;
    char *v2_album;
    char *v2_comment;
    char *v2_year;
       char *v2_genre_string;
    char *lyrics;
    int v2_track_number;
    
    unsigned long int size;
    long int length_in_sec;
    long int bit_rate;
    int samp_rate;
    int track_number;
//    struct mp3_file_info_type *next;
    finialization_functions_list *function_list;
} mp3_file_info;

typedef int (*listing_func_type)( long unsigned size , mp3_file_info *current , unsigned long int max_size ,
                                                        unsigned long int min_size ,unsigned long int max_sec ,unsigned long int min_sec);

#ifndef __WIN32__
#define HINSTANCE long
#endif
typedef struct reader_function_data_type {
	HINSTANCE source_module_instance;        /* will be filled by the main program */
	size_t mp3_file_info_size;
	long mp3list_version;
	listing_func_type reader_func;
/*	mp3_file_info * __stdcall (*reader_func)( long unsigned size , mp3_file_info *current , unsigned long int max_size ,
 											  unsigned long int min_size ,unsigned long int max_sec ,unsigned long int min_sec);*/
	char *file_name_mask;
	char *file_type_description;
	struct reader_function_data_type *next;  /* will be filled by the main program */
	int list_index;                          /* will be filled by the main program */
	bool enabled;                            /* will be filled by the main program */
} reader_function_data;

typedef reader_function_data * (*plugin_registering_func_type)();

#define GENRE_MAGIC 128

#define MP3INFO_ENOMEM -1
#define MP3INFO_EBADFILE -2
#define MP3INFO_ECORRUPTEDFILE -3
#define MP3INFO_EIO -4
#define MP3INFO_EFILTERED -5




#endif

