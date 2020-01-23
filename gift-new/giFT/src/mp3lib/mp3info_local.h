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
#ifndef MP3LIST_LOCAL_H
#define MP3LIST_LOCAL_H

#define DESTRUCTUR_FUNCTION_MAGIC 0xdead1231

typedef struct id3_fields_found_t {
	long magic;
	bool title;
	bool artist;
	bool album;
	bool comment;
	bool year;
	bool genre;
	
	bool v2_title;
	bool v2_artist;
	bool v2_album;
	bool v2_comment;
	bool v2_year;
	bool v2_genre;
	
	
}id3_fields_found;

void free_allocated_string(mp3_file_info *file_inf ,  id3_fields_found *fields );
long int get_length( mp3_file_info *file_inf , FILE *checked_file , long int size);



#endif
