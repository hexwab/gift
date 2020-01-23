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
//---------------------------------------------------------------------------
//  One small additional request:
//   PLEASE DON'T REMOVE the following copyright string :
const char CopyrightString[] =
"\nmp3info by Shachar Raindel\nPart of mp3pls ( http://mp3pls.com/ )\n";
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef __WIN32__
#include <windows.h>
#endif
#include "dxhead.h"
#include "mp3info.h"
#include "mp3info_local.h"
#pragma hdrstop
#include "id3v2.h"

#ifdef BORLANDCPP

extern "C" __declspec(dllexport) reader_function_data * __stdcall register_yourself();
extern "C" __declspec(dllexport) int __stdcall read_mp3_data( long unsigned size , mp3_file_info *current , unsigned long int max_size,unsigned long int min_size ,unsigned long int max_sec ,unsigned long int min_sec);

#else

extern "C" reader_function_data * __stdcall register_yourself();
extern "C" int __stdcall read_mp3_data( long unsigned size , mp3_file_info *current , unsigned long int max_size,unsigned long int min_size ,unsigned long int max_sec ,unsigned long int min_sec);

#endif

reader_function_data this_unit_reader_data;

const unsigned long samples_rates_frequencies[3][4] =
{{44100, 48000, 32000, 1},
 {22050, 24000, 16000, 1},
 {11025, 12000,  8000, 1}};


const int tabsel_123[2][3][16] = {
   { {-1,32,64,96,128,160,192,224,256,288,320,352,384,416,448,-1},
     {-1,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,-1},
     {-1,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,-1} },

   { {-1,32,48,56,64,80,96,112,128,144,160,176,192,224,256,-1},
     {-1,8,16,24,32,40,48,56,64,80,96,112,128,144,160,-1},
     {-1,8,16,24,32,40,48,56,64,80,96,112,128,144,160,-1} }
};



const char *ID3V2_HEADER_MAGIC = "ID3";

char *NO_ID3_BANNER = " No Info";

#ifdef BORLANDCPP

#pragma argsused
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fwdreason, LPVOID lpvReserved)
{
        return 1;
}
#endif
//---------------------------------------------------------------------------
#ifdef BORLANDCPP
__declspec(dllexport)
#endif
reader_function_data * __stdcall register_yourself()
{
    int dummy;
    dummy = strlen(CopyrightString);
    this_unit_reader_data.reader_func = read_mp3_data;
    this_unit_reader_data.file_name_mask = "*.mp?";
    this_unit_reader_data.file_type_description = "MPEG Audio File (mp3)";
    this_unit_reader_data.mp3_file_info_size = sizeof(mp3_file_info);
    this_unit_reader_data.mp3list_version = MP3INFO_H_VERSION;
    if(dummy < 60)
    {
        return NULL;
    }
    return &this_unit_reader_data;
}

void free_allocated_string(mp3_file_info *file_inf ,  id3_fields_found *fields )
{
    /*v1 handeling:*/
    if(fields->title)
    {
	fields->title = false;
	free(file_inf->title);
    }
    if(fields->artist)
    {
	fields->artist = false;
	free(file_inf->artist);
    }
    if(fields->album)
    {
	fields->album = false;
	free(file_inf->album);
    }
    if(fields->comment)
    {
	fields->comment = false;
	free(file_inf->comment);
    }
    if(fields->year)
    {
	fields->year = false;
	free(file_inf->year);
    }
    if(fields->genre)
    {
	fields->genre = false;
	free(file_inf->genre_string);
    }
    /*v2 handeling*/
    if(fields->v2_title)
    {
	fields->v2_title = false;
	free(file_inf->v2_title);
    }
    if(fields->v2_artist)
    {
	fields->v2_artist = false;
	free(file_inf->v2_artist);
    }
    if(fields->v2_album)
    {
	fields->v2_album = false;
	free(file_inf->v2_album);
    }
    if(fields->v2_comment)
    {
	fields->v2_comment = false;
	free(file_inf->v2_comment);
    }
    if(fields->v2_year)
    {
	fields->v2_year = false;
	free(file_inf->v2_year);
    }
    if(fields->v2_genre)
    {
	fields->v2_genre = false;
	free(file_inf->v2_genre_string);
    }
}


void free_struct_items(struct finialization_functions_list_tag *function_struct , mp3_file_info *the_struct)
{
	id3_fields_found *file_fields_allocated;
	file_fields_allocated = (id3_fields_found *)function_struct->private_data;
	if(file_fields_allocated->magic == DESTRUCTUR_FUNCTION_MAGIC)
	{
		free_allocated_string(the_struct , file_fields_allocated);
		delete file_fields_allocated;
	}
	delete function_struct;
}

void turncat_id3_string( char *string , int i)
{
    for( ;  (i > -1) && (isspace(string[i]) || (string[i] == 0)) ; i--);
    string[ i + 1] = 0;
}


#ifdef BORLANDCPP
__declspec(dllexport)
#endif
 int __stdcall read_mp3_data( long unsigned size , mp3_file_info *current , unsigned long int max_size,unsigned long int min_size ,unsigned long int max_sec ,unsigned long int min_sec)
{
    FILE *checked_file;
    int i;
    int scan1 , scan2 , scan3;
    char *start_dir_local_id3;
    char id3_buffer[40];
    finialization_functions_list *finalization_func;
    id3_fields_found *file_fields_found;
    file_fields_found = new id3_fields_found;
    if(file_fields_found == NULL)
    {
	return MP3INFO_ENOMEM;
    }
    finalization_func = new finialization_functions_list;
    if(finalization_func == NULL)
    {
	delete file_fields_found;
	return MP3INFO_ENOMEM;
    }
    finalization_func->function = free_struct_items;
    file_fields_found->magic = DESTRUCTUR_FUNCTION_MAGIC;
    file_fields_found->title = file_fields_found->artist =
	file_fields_found->album = file_fields_found->comment =
        file_fields_found->year = file_fields_found->genre = false;
    file_fields_found->v2_title = file_fields_found->v2_artist =
	file_fields_found->v2_album = file_fields_found->v2_comment =
        file_fields_found->v2_year = file_fields_found->v2_genre = false;
    finalization_func->private_data = (void *)file_fields_found;
    finalization_func->next = current->function_list;
    current->function_list = finalization_func;
    current->genre = 0;
    start_dir_local_id3 = (char *)malloc(strlen(current->file_path)+strlen(current->file_name)+10);
    if(start_dir_local_id3 == NULL)
    {
	delete file_fields_found;
	delete finalization_func;
	return MP3INFO_ENOMEM;
    }
    strcpy(start_dir_local_id3 , current->file_path);
    strcat(start_dir_local_id3 , current->file_name );
    if ((checked_file = fopen(start_dir_local_id3  , "rb" )) == NULL)
    {
	current->function_list = finalization_func->next;
	delete file_fields_found;
	delete finalization_func;
	free(start_dir_local_id3);
	return MP3INFO_EIO; //Skip the file
    }
    read_id3v2(current , checked_file  , size ,  file_fields_found);
    if(get_length( current , checked_file , size)==-1)
    {
	/* The cleanup is done outside*/
	fclose(checked_file);
	free(start_dir_local_id3);
	return MP3INFO_EBADFILE;
    }
    if((max_sec?current->length_in_sec > max_sec:0) || (min_sec?current->length_in_sec < min_sec:0))
    {
	fclose(checked_file);
	free(start_dir_local_id3);
	return MP3INFO_EFILTERED;
    }
    if(size != 0)
	fseek(checked_file , size - 128 , SEEK_SET);
    scan1 = fgetc(checked_file);
    scan2 = fgetc(checked_file);
    scan3 = fgetc(checked_file);
    current->size = (size/1024);
    if(((scan1 != 'T') || (scan2 != 'A') || (scan3 != 'G') || (size == 0)))
    {
	current->id3v1found = false;
	if(!(file_fields_found->title))
	{
	    current->title = NO_ID3_BANNER; //The space in the start is to make sure that those files will be grouped in the start
	}
	if(!(file_fields_found->artist))
	{
	    current->artist = NO_ID3_BANNER;
	}
	if(!(file_fields_found->album))
	{
	    current->album = NO_ID3_BANNER;
	}
	if(!(file_fields_found->comment))
	{
	    current->comment = NO_ID3_BANNER;
	}
	if(!(file_fields_found->year))
	{
	    current->year = NO_ID3_BANNER;
	}
	if(!(file_fields_found->genre))
	{
	    current->genre = 0;
	}
    }else {
	current->id3v1found = true;
	fgets(id3_buffer , 31 , checked_file);
	turncat_id3_string(id3_buffer , 30);
	if(!(file_fields_found->title))
	{
	    current->title = strdup(id3_buffer);
	    file_fields_found->title = (current->title != NULL);
	}
	fgets(id3_buffer , 31 , checked_file);
	turncat_id3_string(id3_buffer , 30);
	if(!(file_fields_found->artist))
	{
	    current->artist = strdup(id3_buffer);
	    file_fields_found->artist = (current->artist != NULL);
	}
	fgets(id3_buffer , 31 , checked_file);
	turncat_id3_string(id3_buffer , 30);
	if(!(file_fields_found->album))
	{
	    current->album = strdup(id3_buffer);
	    file_fields_found->album = (current->album != NULL);
	}
	fgets(id3_buffer , 5 , checked_file);
	turncat_id3_string(id3_buffer , 4);
	if(!(file_fields_found->year))
	{
	    current->year = strdup(id3_buffer);
	    file_fields_found->year = (current->year != NULL);
	}
	fgets(id3_buffer , 31 , checked_file);
	if(id3_buffer[28] == 0)
    	{
	    i = 27;
	    current->track_number = id3_buffer[29];  /* ID3v1.1 support..... How difficult.... */
    	}
 else
	{
    	    i = 30;
	    current->track_number = 0;
	}
	turncat_id3_string(id3_buffer , i);
	if(!(file_fields_found->comment))
	{
	    current->comment = strdup(id3_buffer);
	    file_fields_found->comment = (current->comment != NULL);
	}
	if(!(file_fields_found->genre)&&!(file_fields_found->v2_genre))
	{
	    current->genre = getc(checked_file);
	    current->genre = current->genre > 125 ? 127 : current->genre + 1;
	}
    }
    fclose(checked_file);
    free(start_dir_local_id3);
    return 1;
}



#define MAX_BAD_FRAMES (100000L)   /* This IS enough - anything with more then this number of corrupted frames is random data */


long int get_length( mp3_file_info *file_inf , FILE *checked_file , long int size)
{
    unsigned char scan1 , scan2 ;
	bool good_file=true;
	XHEADDATA XingHeader;
    char mpeg_v , layer , bit_rate_index; 
    char old_mpeg_v , old_layer , old_bit_rate_index;    
    unsigned char buf[200] ;
    long int bit_rate;
//  long int id3v2_tag_size;
    unsigned long int  i = 0 , sample_rate , old_sample_rate , bad_frames_count = 0;
    double tmp1 , tmp2 ;
    scan1 = 0;
    do
    {
	if(bad_frames_count > MAX_BAD_FRAMES)
	{
	    good_file = false;
	    break;
	}
	do
	{
	    /*-----------------09/12/01 17:02-------------------
	     * Find the MPEG audio synchronization bits
	     * --------------------------------------------------*/
	    do
	    {
		if(scan1 != 0xFF)
		{
		    while(fgetc(checked_file) != 0xFF)
			if(feof(checked_file))
			    break;
		}
		if(feof(checked_file))
		{
		    scan1 = 0x01;
		    break;
		}
		scan1 =  fgetc(checked_file);
	    }while((scan1 & 0xE0) != 0xE0);
	    buf[0] = 0xFF;
	    buf[1] = scan1;
	    if(scan1 == 0x01)
	    {
		good_file = false;
		break;
	    }
	    if(((scan1&0x18) != 0x08)&&((scan1&0x06) != 0x00))
	    {
		break;
	    }
	}while( (good_file) && !feof(checked_file));
	if(feof(checked_file))
        {
            good_file = false;
        }
        if(good_file)
        {
	    /*-----------------09/12/01 17:03-------------------
	     * Abnalyze the frame
	     * --------------------------------------------------*/
	    if(((scan1&0x06)>>1) > 0x03||((scan1&0x06)>>1) < 0x01)
	    {
		bad_frames_count++;
		continue;
	    }
	    layer = 4 - ((scan1&0x06)>>1);
            mpeg_v =  3-((scan1&0x18)>>3);
	    if(mpeg_v == 2)
	    {
		bad_frames_count++;
		continue;
	    }else if(mpeg_v == 3) {
		mpeg_v = 2;
	    }
	    scan2 =fgetc(checked_file);
	    buf[2] =  scan2;
	    bit_rate_index = ((scan2&0xF0)>>4);
	    sample_rate = samples_rates_frequencies[mpeg_v][((scan2&0x0c) >> 2)];
	    if(sample_rate == 1)
	    {
		bad_frames_count++;
		continue;
	    }
            if(bit_rate_index == 0xF )
            {
                bad_frames_count++;
                continue;
            }
            if( bit_rate_index == 0)
            {
                bit_rate = 2000000L;
                break;
            }
            bit_rate = tabsel_123[mpeg_v>0?1:0][layer-1][bit_rate_index];
	    if(bit_rate == -1)
	    {
		bad_frames_count++;
		continue;
	    }
	    for(i=3;i<200;i++)
	    {
		buf[i] = fgetc(checked_file);
		if(feof(checked_file))
		{
		    break;
                }
	    }
            break;

        }else {
            break;
        }
    }while(!feof(checked_file));
    if(good_file)
    {
	/*-----------------09/12/01 17:03-------------------
	 * Check for a XING header
	 * --------------------------------------------------*/
	good_file = true;
	XingHeader.toc = NULL;
 /* we don't want the toc*/	if(mpg123_get_xing_header(&XingHeader , buf))
	{
	    if( XingHeader.frames != -1)
	    {
		file_inf->length_in_sec = XingHeader.frames*(layer==1?384:1152)/sample_rate;
		file_inf->bit_rate = ((size*8)/file_inf->length_in_sec)/1024;
                file_inf->samp_rate = sample_rate;
	    }else
 {
		/* Bad Xing Header - fallback to the normal estimation  */
		/* Better action will be to count the frames in the file - FIX */
		tmp1 = (double)bit_rate * (double)125.0;
		tmp2 = (double)size / tmp1;
		file_inf->length_in_sec = tmp2;
        	file_inf->bit_rate = bit_rate;
                file_inf->samp_rate = sample_rate;
            }
	}else {
	    tmp1 = (double)bit_rate * (double)125.0;
	    tmp2 = (double)size / tmp1;
    	    file_inf->length_in_sec = tmp2;
    	    file_inf->bit_rate = bit_rate;
            file_inf->samp_rate = sample_rate;
	}
    }else {
	good_file = false;
        file_inf->length_in_sec = 0;
        file_inf->bit_rate = 0;
    }

    return good_file?file_inf->length_in_sec:-1;
}



