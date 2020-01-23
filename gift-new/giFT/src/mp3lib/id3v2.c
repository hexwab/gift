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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __WIN32__
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <limits.h>

#include "mp3info.h"
#include "mp3info_local.h"
#include <zlib.h>
#pragma hdrstop
#include "id3v2.h"

const char *genres_list[] =  { "Blues" , "Classic Rock" , "Country" , "Dance" , "Disco" , "Funk" , "Grunge" , "Hip-Hop" , "Jazz" ,
                   "Metal" , "New Age" , "Oldies" , "Other" , "Pop" , "R&B" , "Rap" , "Reggae" , "Rock" , "Techno" ,
                   "Industrial" , "Alternative" , "Ska" , "Death Metal" , "Pranks" , "Soundtrack" , "Euro-Techno" ,
                   "Ambient" , "Trip-Hop" , "Vocal" , "Jazz+Funk" , "Fusion" , "Trance" , "Classical" , "Instrumental" ,
                   "Acid" , "House" , "Game" , "Sound Clip" , "Gospel" , "Noise" , "AlternRock" , "Bass" , "Soul" , "Punk" ,
                   "Space" , "Meditative" , "Instrumental Pop" , "Instrumental Roc" , "Ethnic" , "Gothic" , "Darkwave" ,
                   "Techno-Industria" , "Electronic" , "Pop-Folk" , "Eurodance" , "Dream" , "Southern Rock" , "Comedy" ,
                   "Cult" , "Gangsta" , "Top 40" , "Christian Rap" , "Pop/Funk" , "Jungle" , "Native American" , "Cabaret" ,
                   "New Wave" , "Psychadelic" , "Rave" , "Showtunes" , "Trailer" , "Lo-Fi" , "Tribal" , "Acid Punk" ,
                   "Acid Jazz" , "Polka" , "Retro" , "Musical" , "Rock & Roll" , "Hard Rock" , "Folk" , "Folk-Rock" ,
                   "National Folk" , "Swing" , "Fast Fusion" , "Bebob" , "Latin" , "Revival" , "Celtic" , "Bluegrass" ,
                   "Avantgarde" , "Gothic Rock" , "Progressive Rock" , "Psychedelic Rock" , "Symphonic Rock" , "Slow Rock" ,
                   "Big Band" , "Chorus" , "Easy Listening" , "Acoustic" , "Humour" , "Speech" , "Chanson" , "Opera" ,
                   "Chamber Music" , "Sonata" , "Symphony" , "Booty Bass" , "Primus" , "Porn Groove" , "Satire" , "Slow Jam" ,
                   "Club" , "Tango" , "Samba" , "Folklore" , "Ballad" , "Power Ballad" , "Rhythmic Soul" , "Freestyle" , "Duet" ,
                   "Punk Rock" , "Drum Solo" , "A capella" , "Euro-House" , "Dance Hall" , "Unknown"
};

#define NUMBER_OF_KNOWN_GENRES (sizeof(genres_list)/sizeof(genres_list[0]))


/*-----------------09/12/01 23:59-------------------
 * TODO: FINISH THIS!!!!!!!!!!!!!!!!
 * --------------------------------------------------*/
MY_FILE *my_file_associate_with_memory( void *data , size_t data_len)
{
	MY_FILE *new_my_file_handle;
	new_my_file_handle = new MY_FILE;
	new_my_file_handle->stream_type = MY_FILE_MEM;
	new_my_file_handle->big_u.mem_data.data_start = new_my_file_handle->big_u.mem_data.current_point = data;
	new_my_file_handle->big_u.mem_data.buffer_size = data_len;
	return new_my_file_handle;
}

MY_FILE *my_file_associate_with_file_handle( int file_handle)
{
	MY_FILE *new_my_file_handle;
	new_my_file_handle = new MY_FILE;
	new_my_file_handle->stream_type = MY_FILE_FILE_HANDLER;
	new_my_file_handle->big_u.file_handler_data.handle = file_handle;
	new_my_file_handle->big_u.file_handler_data.current_position = lseek(file_handle,0,SEEK_CUR);
	new_my_file_handle->big_u.file_handler_data.got_eof = false;
	if(new_my_file_handle->big_u.file_handler_data.current_position<0)
	{
	    new_my_file_handle->big_u.file_handler_data.current_position = 0;
	}
	return new_my_file_handle;
}

MY_FILE *my_file_associate_with_file_stream( FILE *stream)
{
	MY_FILE *new_my_file_handle;
	new_my_file_handle = new MY_FILE;
	new_my_file_handle->stream_type = MY_FILE_FILE_STREAM;
	new_my_file_handle->big_u.file_stream_data.file = stream;
	return new_my_file_handle;
}

size_t my_fread(void *input_buffer, size_t size_of_item , size_t number_of_items , MY_FILE *file_handler)
{
	size_t n;
	switch(file_handler->stream_type)
	{
		case MY_FILE_MEM :
		{
			size_t available_data_size;
			available_data_size = file_handler->big_u.mem_data.buffer_size -
					 ((char *)(file_handler->big_u.mem_data.current_point) - (char *)(file_handler->big_u.mem_data.data_start));
			if( available_data_size < number_of_items*size_of_item )
			{
				memcpy(input_buffer , file_handler->big_u.mem_data.current_point , available_data_size);
				(char *)(file_handler->big_u.mem_data.current_point) += available_data_size;
				return available_data_size/size_of_item;
			}
			memcpy(input_buffer , file_handler->big_u.mem_data.current_point , number_of_items*size_of_item);
			(char *)(file_handler->big_u.mem_data.current_point) += number_of_items*size_of_item;
			return number_of_items*size_of_item;
//			break;                      /* NOT needed */
		}
		case MY_FILE_FILE_HANDLER :
		{
			n = read(file_handler->big_u.file_handler_data.handle , input_buffer , number_of_items*size_of_item) / size_of_item ;
			file_handler->big_u.file_handler_data.current_position += n;
			if(n == 0)
			{
			    file_handler->big_u.file_handler_data.got_eof = true;
			}
			return n;
//			break;
		}
		case MY_FILE_FILE_STREAM :
		{
			return fread(input_buffer, size_of_item , number_of_items , file_handler->big_u.file_stream_data.file);
		}
                default:
                {
                        return -1;
                }
	}
        return -1; /* Just to make the dumb borland happy*/
}

int my_getc( MY_FILE *file_handler )
{
	char char_to_return;
	switch(file_handler->stream_type)
	{
		case MY_FILE_MEM :
		{
			if( ((char *)(file_handler->big_u.mem_data.current_point) - (char *)(file_handler->big_u.mem_data.data_start)) > file_handler->big_u.mem_data.buffer_size )
			{
				return EOF;
			}
			char_to_return = *((char *)(file_handler->big_u.mem_data.current_point));
			((char *)(file_handler->big_u.mem_data.current_point))++;
			return char_to_return;
//			break;                      /* NOT needed */
		}
		case MY_FILE_FILE_HANDLER :
		{
			if(read(file_handler->big_u.file_handler_data.handle , &char_to_return , 1)!=1)
			{
			    file_handler->big_u.file_handler_data.got_eof = true;
				return EOF;
			}
			file_handler->big_u.file_handler_data.current_position++;
			return char_to_return;
//			break;
		}
		case MY_FILE_FILE_STREAM :
		{
			return getc(file_handler->big_u.file_stream_data.file);
		}
	}
        return EOF;
}

int my_fgetc( MY_FILE *file_handle )
{
	char char_to_return;
	switch(file_handle->stream_type)
	{
		case MY_FILE_MEM :
		{
			if( ((char *)(file_handle->big_u.mem_data.current_point) - (char *)(file_handle->big_u.mem_data.data_start)) >= file_handle->big_u.mem_data.buffer_size )
			{
				return EOF;
			}
			char_to_return = *((char *)(file_handle->big_u.mem_data.current_point));
			((char *)(file_handle->big_u.mem_data.current_point))++;
			return char_to_return;
//			break;                      /* NOT needed */
		}
		case MY_FILE_FILE_HANDLER :
		{
			if(read(file_handle->big_u.file_handler_data.handle , &char_to_return , 1)!=1)
			{
			    file_handle->big_u.file_handler_data.got_eof = true;
				return EOF;
			}
			file_handle->big_u.file_handler_data.current_position++;
			return char_to_return;
//			break;
		}
		case MY_FILE_FILE_STREAM :
		{
			return fgetc(file_handle->big_u.file_stream_data.file);
		}
                default:
                {
                        return EOF;
                }
	}
        return EOF; /* Just to make the dumb Borland happy*/
}

long my_fseek( MY_FILE *file_handler , long offset , int from_where )
{
	size_t n;
	switch(file_handler->stream_type)
	{
		case MY_FILE_MEM :
		{
			switch ( from_where) {
				case SEEK_CUR :
				{
					size_t available_data_size;
					available_data_size = file_handler->big_u.mem_data.buffer_size -
							 ((char *)(file_handler->big_u.mem_data.current_point) - (char *)(file_handler->big_u.mem_data.data_start));
					if( (available_data_size < offset ) ||
					 (((char *)(file_handler->big_u.mem_data.current_point) - (char *)(file_handler->big_u.mem_data.data_start)) < (0-offset)))
					{
//						errno = EINVAL;
						return -1;
					}
					(char *)(file_handler->big_u.mem_data.current_point) += offset;
					break;
				}
				case SEEK_END :
				{
					if((offset >0)||((0-offset)>file_handler->big_u.mem_data.buffer_size))
					{
//						errno = EINVAL;
						return -1;
					}
					file_handler->big_u.mem_data.current_point = (char *)(file_handler->big_u.mem_data.data_start) + file_handler->big_u.mem_data.buffer_size + offset;
					break;
				}
				case SEEK_SET :
				{
					if((offset<0)||((offset)>file_handler->big_u.mem_data.buffer_size))
					{
//						errno = EINVAL;
						return -1;
					}
					file_handler->big_u.mem_data.current_point = (char *)(file_handler->big_u.mem_data.data_start) + offset;
					break;
				}

			}
			return (char *)(file_handler->big_u.mem_data.current_point) - (char *)(file_handler->big_u.mem_data.data_start) ;
//			break;                      /* NOT needed */
		}
		case MY_FILE_FILE_HANDLER :
		{
			n = lseek(file_handler->big_u.file_handler_data.handle , offset ,from_where);
			if(n != -1)
			{
				file_handler->big_u.file_handler_data.current_position = n;
				return n;
			}else {
				return file_handler->big_u.file_handler_data.current_position;
			}

//			break;
		}
		case MY_FILE_FILE_STREAM :
		{
			return fseek(file_handler->big_u.file_stream_data.file , offset ,from_where);
		}
	}
        return -1;
}


size_t my_ftell(MY_FILE *file_handler)
{
	switch(file_handler->stream_type)
	{
		case MY_FILE_MEM :
		{
			return (char *)(file_handler->big_u.mem_data.current_point) - (char *)(file_handler->big_u.mem_data.data_start) ;
//			break;
		}
		case MY_FILE_FILE_HANDLER :
		{
			return 	file_handler->big_u.file_handler_data.current_position;

//			break;
		}
		case MY_FILE_FILE_STREAM :
		{
			return ftell(file_handler->big_u.file_stream_data.file);
//			break;
		}
	}
        return -1; /* just to make the dumb Borland happy*/
}


int my_file_feof( MY_FILE *file_handler )
{
	switch(file_handler->stream_type)
	{
		case MY_FILE_MEM :
		{
			if( ((char *)(file_handler->big_u.mem_data.current_point) - (char *)(file_handler->big_u.mem_data.data_start)) >= file_handler->big_u.mem_data.buffer_size )
			{
				return 1;
			}
			return 0;
		}
		case MY_FILE_FILE_HANDLER :
		{
			return file_handler->big_u.file_handler_data.got_eof;
//			break;
		}
		case MY_FILE_FILE_STREAM :
		{
			return feof(file_handler->big_u.file_stream_data.file);
//			break;
		}
	}
        return 1;
}

void my_file_free( MY_FILE *file_handler )
{
	delete file_handler;
}

void my_file_close(MY_FILE *file_handler)
{
	switch(file_handler->stream_type)
	{
		case MY_FILE_MEM :
			break;
		case MY_FILE_FILE_HANDLER :
		{
			close(file_handler->big_u.file_handler_data.handle);
			break;
		}
		case MY_FILE_FILE_STREAM :
		{
			fclose(file_handler->big_u.file_stream_data.file);
			break;
		}
	}
        my_file_free(file_handler);
//        return 1;
}




size_t unsyncer_fread(void *input_buffer , size_t size_of_item , size_t number_of_items , MY_FILE *stream , bool do_unsync , size_t *realy_read , size_t *data_size , bool count_unsync_bytes)
{
	register unsigned char *out_ptr;
	register size_t total_bytes_left_to_read;
	register size_t local_realy_read;
	size_t local_data_size;
	size_t bytes_asked_to_read;
	if(!do_unsync)
	{
		total_bytes_left_to_read = my_fread(input_buffer , size_of_item ,number_of_items , stream);
		if(realy_read != NULL)
		{
			*realy_read = size_of_item * total_bytes_left_to_read;
		}
        	if(data_size != NULL)
	        {
        		*data_size = size_of_item * total_bytes_left_to_read;
	        }
		return total_bytes_left_to_read;
	}
	/*-----------------10/12/01 16:35-------------------
	 * TODO: rewrite it more optimal
	 * --------------------------------------------------*/
	out_ptr = (unsigned char *)input_buffer;
	local_realy_read = 0;
	local_data_size = 0;
	for(bytes_asked_to_read = total_bytes_left_to_read = size_of_item * number_of_items;total_bytes_left_to_read>0;total_bytes_left_to_read--)
	{
		if(my_file_feof(stream))
		{
			break;
		}
		*out_ptr = my_fgetc(stream);
		local_realy_read++;             /* Pipeline hazard due to jumps will cost more then one inc operation  */
		local_data_size++;
		if((*out_ptr ==0x00)&&(*(out_ptr-1) == 0xFF))
		{
			if(my_file_feof(stream))
			{

				if(!count_unsync_bytes)
				{
					total_bytes_left_to_read++;
				}
				break;
			}
			*out_ptr = my_fgetc(stream);
			if(count_unsync_bytes)
			{
				total_bytes_left_to_read--;
			}
			local_realy_read++;
		}
		out_ptr++;
	}
	if(realy_read != NULL)
	{
		*realy_read = local_realy_read;
	}
	if(data_size != NULL)
	{
		*data_size = local_data_size;
	}
	return (bytes_asked_to_read-total_bytes_left_to_read)/size_of_item;
}

/*-----------------10/12/01 23:49-------------------
 * Reades ID3v2 teg (without headers) from stream,
 * and add all of its frames into one linked list.
 * --------------------------------------------------*/

#define MAX(a,b) a>b?a:b

int parse_id3v2( MY_FILE *checked_file  , size_t size , bool unsync_used , size_t tag_size , id3v2_tag *tag_data , char major_id3version)
{
	char frame_header_buffer[MAX(ID3v2_DOT_2_FRAME_HEADER_SIZE , ID3v2_DOT_3_FRAME_HEADER_SIZE)] , *frame_buffer;
	size_t total_read = 0 ;
	size_t frame_size;
	unsigned long int frame_type;
	bool last_frame_valid = true;
	size_t bytes_read;
	size_t real_frame_data_size;
	id3v2frame *new_frame;
        int i;
#if 0
	if(header_buffer[5] & 0x80)
	{
		unsync_used = true;
	}
	if(header_buffer[5] & 0x40)
	{
		if(major_id3version == 0x02)
		{
			return 0 ;                        /* Invalid Tag - Skip */
		}else if(major_id3version >= 0x03 )
		{
			/*-----------------11/12/01 17:51-------------------
			 * Extended header available, skip
			 * --------------------------------------------------*/

		}
	}
#endif
	switch ( major_id3version) {
		case 0x02 :
		case 0x03 :
			break;
		default:
			return 0;                   /* We can't handle this tag, so skip over it. */
	}

	while(total_read < tag_size)
	{
		if(unsyncer_fread(frame_header_buffer , 1 , (major_id3version==0x02)?ID3v2_DOT_2_FRAME_HEADER_SIZE:ID3v2_DOT_3_FRAME_HEADER_SIZE , checked_file , unsync_used , &bytes_read , NULL , false ) != ((major_id3version==0x02)?ID3v2_DOT_2_FRAME_HEADER_SIZE:ID3v2_DOT_3_FRAME_HEADER_SIZE))
		{
			return MP3INFO_ECORRUPTEDFILE ;
		}
		total_read += bytes_read;
		if(major_id3version==0x02)
		{
			frame_type = MAKE_INT_FROM_3_CHARS( frame_header_buffer[0] , frame_header_buffer[1] , frame_header_buffer[2]);
		}else {
			frame_type = MAKE_INT_FROM_4_CHARS( frame_header_buffer[0] , frame_header_buffer[1] , frame_header_buffer[2] , frame_header_buffer[3]);
		}
		last_frame_valid = false;
		for(i=0; i<((major_id3version==0x02)?ID3v2_DOT_2_KNOWN_FRAMES_NUMBER:ID3v2_DOT_3_KNOWN_FRAMES_NUMBER) ; i++)
		{
			if(((major_id3version==0x02)?known_id3v2_dot_2_frames[i]:known_id3v2_dot_3_frames[i]) == frame_type)
			{
				last_frame_valid = true;
				break;
			}
		}
		if(last_frame_valid == false)
		{
			/*-----------------10/12/01 22:10-------------------
			 * There may be padding, so we leave now
			 * --------------------------------------------------*/
			break;
		}
		if(major_id3version==0x02)
		{
			frame_size = (frame_header_buffer[3] << 16) | (frame_header_buffer[4] << 8) | (frame_header_buffer[5]);
		}else /*if(major_id3version==0x03)*/
		{
			frame_size = (frame_header_buffer[4] << 24) | (frame_header_buffer[5] << 16) | (frame_header_buffer[6] << 8) | (frame_header_buffer[7]);
		}
		/*-----------------10/12/01 16:59-------------------
		 * Doc is unclear - is the size is for the unsynced data
		 * or for the original data?? I choose the unsynced, according to v2.4
		 * --------------------------------------------------*/
		/*-----------------10/12/01 22:12-------------------
		 * Read the frame data
		 * --------------------------------------------------*/
                if(frame_size == 0)
                {
                        break;
                }
		frame_buffer = (char *)malloc(frame_size + 2);
		if(frame_buffer == NULL)
		{
			return MP3INFO_ENOMEM;
		}

		if(total_read +frame_size > tag_size)
		{
			free(frame_buffer);
			return MP3INFO_ECORRUPTEDFILE ;
		}
		if(unsyncer_fread(frame_buffer , 1 , frame_size , checked_file , unsync_used , &bytes_read , &real_frame_data_size , true ) != frame_size)
		{
			free(frame_buffer);
			return MP3INFO_ECORRUPTEDFILE ;
		}
		total_read += bytes_read;
		if(total_read > tag_size)
		{
			free(frame_buffer);
			return MP3INFO_ECORRUPTEDFILE ;
		}
		if((major_id3version==0x02)&&(frame_type == MAKE_INT_FROM_3_CHARS( 'C' , 'D' , 'M' )))
		{
			/*-----------------11/12/01 20:36-------------------
			 * Handle ID3v2.2.1 CDM frame
			 * --------------------------------------------------*/
			if(*frame_buffer == 'z')
			{
				void *uncompressed_buffer;
				unsigned long uncompressed_size;
				int sub_parse_return_value;
				unsigned char id3v2buffer[ID3V2_MAIN_HEADER_SIZE];
				MY_FILE *uncompressed_meta_tag;
				uncompressed_size = (frame_buffer[0]<<24)|(frame_buffer[1]<<16)|(frame_buffer[2]<<8)|frame_buffer[3];
				uncompressed_buffer = malloc(uncompressed_size);
				if(uncompressed_buffer == NULL)
				{
					free(frame_buffer);
					return MP3INFO_ENOMEM;
				}
				uncompress((Bytef *)(uncompressed_buffer) , &uncompressed_size ,(const Bytef *) &(frame_buffer[1+4]) , real_frame_data_size - 1 - 4);
				uncompressed_meta_tag = my_file_associate_with_memory(uncompressed_buffer , uncompressed_size);
				sub_parse_return_value = parse_id3v2( uncompressed_meta_tag  ,
						 UINT_MAX /* compressed tag can be larger the the entire file, so bypass the sanity checks*/ ,
						  false , uncompressed_size , tag_data , major_id3version);
				my_file_close(uncompressed_meta_tag);
				free(uncompressed_buffer);
				free(frame_buffer);
			}
		}else {

			/*-----------------10/12/01 22:08-------------------
			 * build a frame block, and add it to the list
			 * --------------------------------------------------*/
			bool skip_frame = false;
			new_frame = (id3v2frame *)malloc(sizeof(id3v2frame));
			if(new_frame == NULL)
			{
				free(frame_buffer);
				return MP3INFO_ENOMEM;
			}
			if(major_id3version==0x02)
			{
				frame_buffer[real_frame_data_size] = frame_buffer[real_frame_data_size+1] = 0;  /* to make string handeling easier */
				new_frame->fr_data = frame_buffer;
				new_frame->fr_data_len = real_frame_data_size;
				skip_frame = false;
			}else if(major_id3version==0x03)
			{
				skip_frame = false;
				if((frame_header_buffer[9] & ~(0xA0))!=0)
				{
					skip_frame=true;
				}
				if(frame_header_buffer[9] & 0x80)
				{
					long unsigned uncompressed_size;
					char *uncompressed_data;
					uncompressed_size = (frame_buffer[0]<<24)|(frame_buffer[1]<<16)|(frame_buffer[2]<<8)|frame_buffer[3];
					uncompressed_data = (char *)malloc(uncompressed_size+2);
					if(uncompressed_data == NULL)
					{
						free(frame_buffer);
						free(new_frame);
						return MP3INFO_ENOMEM;
					}
					uncompress((Bytef *)uncompressed_data , &uncompressed_size ,
							 (const Bytef *)&(frame_buffer[4+((frame_header_buffer[9] & 0x20)?1:0)]) ,
							 real_frame_data_size - ((frame_header_buffer[9] & 0x20)?1:0) - 4);
					free(frame_buffer);
					frame_buffer = uncompressed_data;
					real_frame_data_size = uncompressed_size;
				}
				frame_buffer[real_frame_data_size] = frame_buffer[real_frame_data_size+1] = 0;  /* to make string handeling easier */
				new_frame->fr_data = frame_buffer;
				new_frame->fr_data_len = real_frame_data_size;
			}else {
				/*-----------------22/12/01 12:28-------------------
				 * TODO: Handel ID3v2.4
				 * --------------------------------------------------*/
				skip_frame = true;
			}
			if(!skip_frame)
			{
				new_frame->id3v2_major_version = major_id3version;
				new_frame->frame_id = frame_type;
				new_frame->next = tag_data->first_frame;
				new_frame->previous = NULL;
 				if(tag_data->first_frame != NULL)
				{
				   tag_data->first_frame->previous = new_frame;
				}
				tag_data->first_frame = new_frame;
				if(tag_data->last_frame == NULL)
				{
					tag_data->last_frame = new_frame;
				}
			}else {
				free(frame_buffer);
				free(new_frame);
                        }
		}
	}
	return 0;
}

id3v2frame *find_frame(id3v2_tag *tag_data , unsigned long long_frame_id , unsigned long short_frame_id , int frames_to_skip)
{
	int i;
	id3v2frame *current_suspect , *last_found;
	last_found = NULL;
	for(current_suspect = tag_data->last_frame /* We start from the end, because this is the first tag in the actual file*/ ;
	    current_suspect != NULL ; current_suspect = current_suspect->previous)
	{
		if(((current_suspect->id3v2_major_version ==0x02)&&( current_suspect->frame_id == short_frame_id))||
		   ((current_suspect->id3v2_major_version ==0x03)&&( current_suspect->frame_id == long_frame_id)))
		{
			last_found = current_suspect;
			if(frames_to_skip == 0)
			{
				break;
			}
			frames_to_skip--;
		}
	}
	return last_found;
}

char *make_4_char_from_int(long frame_id)
{
        static char internal_buffer[5];
        internal_buffer[0] =    (frame_id >>24) & 0xff;
        internal_buffer[1] =    (frame_id >>16) & 0xff;
        internal_buffer[2] =    (frame_id >>8) & 0xff;
        internal_buffer[3] =    (frame_id ) & 0xff;
        internal_buffer[4] = 0;
        return internal_buffer;
}

char *make_3_char_from_int(long frame_id)
{
        static char internal_buffer[4];
        internal_buffer[0] =    (frame_id >>16) & 0xff;
        internal_buffer[1] =    (frame_id >>8) & 0xff;
        internal_buffer[2] =    (frame_id ) & 0xff;
        internal_buffer[3] = 0;
        return internal_buffer;
}

void dump_id3v2_tag(id3v2_tag *tag_data)
{
	id3v2frame *current_suspect ;
        int i;
        char *frame_data;
        char *frame_type;
	for(current_suspect = tag_data->last_frame /* We start from the end, because this is the first tag in the actual file*/ ;
	    current_suspect != NULL ; current_suspect = current_suspect->previous)
	{
                if(current_suspect->id3v2_major_version == 0x02)
                {
                        frame_type = make_3_char_from_int(current_suspect->frame_id);
                        printf("id3v2.2 frame found. frame ID: %s. frame data:\n" ,frame_type);
                        frame_data = current_suspect->fr_data;
                        for(i = 0;i<current_suspect->fr_data_len;i++)
                        {
                                printf("%c(0x%X)" , *frame_data , (int)*frame_data);
                                frame_data++;
                                if(i%10 == 0)
                                {
                                        printf("\n");
                                }
                        }
                        printf("\n\n");
                }else if(current_suspect->id3v2_major_version == 0x03)
                {
                        frame_type = make_4_char_from_int(current_suspect->frame_id);
                        printf("id3v2.3 frame found. frame ID: %s. frame data:\n" ,frame_type);
                        frame_data = current_suspect->fr_data;
                        for(i = 0;i<current_suspect->fr_data_len;i++)
                        {
                                printf("%c(0x%X)" , *frame_data , (int)*frame_data);
                                frame_data++;
                                if(i%10 == 0)
                                {
                                        printf("\n");
                                }
                        }
                        printf("\n\n");
                }
        }

}

char *get_text_from_frame(id3v2frame *the_frame)
{
	char *return_string;
	int the_string_length;
	if(*(the_frame->fr_data) == 0x00 /* ASCII*/)
	{
		return_string = (char *)malloc(the_frame->fr_data_len);
		if(return_string == NULL)
		{
			return NULL;
		}
		strcpy(return_string , (the_frame->fr_data)+1 );
	}else

#ifdef __WIN32__
	 if(*(the_frame->fr_data) == 0x01 /* UNICODE*/)
	{

	 	the_string_length =  WideCharToMultiByte(

    	 	CP_ACP ,    	 // code page
	     	0 ,    	 // performance and mapping flags
    	 	(const wchar_t *)((the_frame->fr_data)+1) ,    	 // address of wide-character string
     		-1 ,    	 // number of characters in string
	     	0 ,    	 // address of buffer for new string
    	 	0 ,    	 // size of buffer
     		NULL ,    	 // address of default for unmappable characters
	     	NULL      	 // address of flag set when default char. used
    	);
			if(the_string_length != 0)
		{
			return_string = (char *)malloc(the_string_length);
			if(return_string == NULL)
			{
				return NULL;
			}
		 	WideCharToMultiByte(

    				CP_ACP ,    	 // code page
		     		0 ,    	 // performance and mapping flags
    		 		(const wchar_t *)((the_frame->fr_data)+1) ,    	 // address of wide-character string
	    	 		-1 ,    	 // number of characters in string
	     			return_string ,    	 // address of buffer for new string
	     			the_string_length ,    	 // size of buffer
	    	 		NULL ,    	 // address of default for unmappable characters
		     		NULL      	 // address of flag set when default char. used
    		);
		}
	}else /* NOT SUPPORTED */
#endif	
	{
		return_string = NULL;
	}
	return return_string;

}

const char *RemixString = "Remix";
const char *RemixIDString = "RX";
const char *CoverString = "Cover Version";
const char *CoverIDString = "CR";
const char *AFTER_GENERE_BANNER = " - ";


const char *get_genre_from_table_id_string(char *id)
{
	int genre_number = 0;
	char *number_reader_ptr;
	if(id == NULL)
	{
		return NULL;
	}
	if(strcmp(id , RemixIDString) == 0)
	{
		return RemixString;
	}
	if(strcmp(id , CoverIDString) == 0)
	{
		return CoverString;
	}
	for(number_reader_ptr = id; isdigit(*number_reader_ptr) && *number_reader_ptr != 0; number_reader_ptr++)
	{
		genre_number *= 10;
		genre_number += *number_reader_ptr - '0';
	}
	if((*number_reader_ptr != 0) || (genre_number > NUMBER_OF_KNOWN_GENRES))
	{
		return NULL;
	}
	return genres_list[genre_number];
}

char *get_content_from_frame(id3v2frame *the_frame)
{
	char *original_string, *final_string , *tmp_ptr , *number_ptr , *tmp_ptr2 , *final_string_ptr;
	char *genre_string_from_table;
	char number_buffer[100];            /* MAGIC! MAGIC! (but hopefully large enough - long long is 2^64 ~~ 10^20) */
	size_t final_string_length;
	final_string_length = 0;
	original_string = get_text_from_frame(the_frame);
	if(original_string == NULL)
	{
		return NULL;
	}
	/*-----------------22/12/01 14:55-------------------
	 * Esstimate the size of the string
	 * --------------------------------------------------*/
	tmp_ptr = original_string;
	while(*tmp_ptr != 0)
	{
		if(*tmp_ptr == '(')
		{
			if(*(tmp_ptr+1) == '(')
			{
				tmp_ptr += 2;
				final_string_length++;
			}else {
				number_ptr = number_buffer;
				tmp_ptr2 = tmp_ptr+1;
				while((*tmp_ptr2 != 0)&&(*tmp_ptr2 != ')'))
				{
					*number_ptr++ = *tmp_ptr2++;
				}
				*number_ptr = 0;
				if(*tmp_ptr2 != 0)
				{
					genre_string_from_table = (char *)get_genre_from_table_id_string(number_buffer);
					if(genre_string_from_table != NULL)
					{
						final_string_length += strlen(genre_string_from_table);
						tmp_ptr = tmp_ptr2+1;
						if(*tmp_ptr != 0)
						{
							final_string_length += strlen(AFTER_GENERE_BANNER);
						}
					}else {
						/*-----------------22/12/01 14:59-------------------
						 * If we don't know how to eat the string, put it as it
						 * --------------------------------------------------*/
						final_string_length++;
						tmp_ptr++;
					}
				}else {
					/*-----------------22/12/01 12:51-------------------
					 * Bugos reference to predefined genre
					 * --------------------------------------------------*/
					tmp_ptr++;
					final_string_length++;
				}
			}
		}else {
			tmp_ptr++;
			final_string_length++;
		}
	}
	final_string_length++;              /* for the NULL terminator */
	final_string_ptr = final_string = (char *)malloc(final_string_length);
	if(final_string == NULL)
	{
		free(original_string);
		return NULL;
	}
	/*-----------------22/12/01 14:58-------------------
	 * actually build the string
	 * --------------------------------------------------*/
	tmp_ptr = original_string;
	while(*tmp_ptr != 0)
	{
		if(*tmp_ptr == '(')
		{
			if(*(tmp_ptr+1) == '(')
			{
				tmp_ptr += 2;
				*final_string_ptr++ = '(';
			}else {
				number_ptr = number_buffer;
				tmp_ptr2 = tmp_ptr+1;
				while((*tmp_ptr2 != 0)&&(*tmp_ptr2 != ')'))
				{
					*number_ptr++ = *tmp_ptr2++;
				}
				*number_ptr = 0;
				if(*tmp_ptr2 != 0)
				{
					genre_string_from_table = (char *)get_genre_from_table_id_string(number_buffer);
					if(genre_string_from_table != NULL)
					{
						tmp_ptr = tmp_ptr2+1;
						for(tmp_ptr2 = genre_string_from_table;*tmp_ptr2 != 0;tmp_ptr2++)
						{
							*final_string_ptr++ = *tmp_ptr2;
						}
						if(*tmp_ptr != 0)
						{
							strncpy(final_string_ptr , AFTER_GENERE_BANNER , strlen(AFTER_GENERE_BANNER));
							final_string_ptr += strlen(AFTER_GENERE_BANNER);
						}
					}else {
						/*-----------------22/12/01 14:59-------------------
						 * If we don't know how to eat the string, put it as it
						 * --------------------------------------------------*/
						tmp_ptr++;
						*final_string_ptr++ = '(';
					}
				}else {
					/*-----------------22/12/01 12:51-------------------
					 * Bugos reference to predefined genre
					 * --------------------------------------------------*/
					tmp_ptr++;
					*final_string_ptr++ = '(';
				}
			}
		}else {
			*final_string_ptr++ = *tmp_ptr++;
		}
	}
	*final_string_ptr = 0;
	free(original_string);
	return final_string;
}



#define TP1_BIT (1<<0)
#define TP2_BIT (1<<1)
#define TP3_BIT (1<<2)
#define TP4_BIT (1<<3)

#define TT1_BIT (1<<0)
#define TT2_BIT (1<<1)
#define TT3_BIT (1<<2)


void copy_id3v2_into_file_info(id3v2_tag *tag_data , mp3_file_info *file_inf , id3_fields_found *fields)
{
	id3v2frame *tp1_frame , *tp2_frame , *tp3_frame , *tp4_frame , *tt1_frame ,
			 *tt2_frame , *tt3_frame , *talb_frame , *tcon_frame ;
	char *tp1_text , *tp2_text , *tp3_text , *tp4_text , *tt1_text , *tt2_text , *tt3_text , *talb_text , *tcon_text;
	long artist_info_mask = 0 ,  title_info_mask = 0;
	tp1_frame = find_frame(tag_data , ID3v2_DOT_3_FRAME_ID_TPE1 , ID3v2_DOT_2_FRAME_ID_TP1 , 0);
	tp2_frame = find_frame(tag_data , ID3v2_DOT_3_FRAME_ID_TPE2 , ID3v2_DOT_2_FRAME_ID_TP2 , 0);
	tp3_frame = find_frame(tag_data , ID3v2_DOT_3_FRAME_ID_TPE3 , ID3v2_DOT_2_FRAME_ID_TP3 , 0);
	tp4_frame = find_frame(tag_data , ID3v2_DOT_3_FRAME_ID_TPE4 , ID3v2_DOT_2_FRAME_ID_TP4 , 0);

	tt1_frame = find_frame(tag_data , ID3v2_DOT_3_FRAME_ID_TIT1 , ID3v2_DOT_2_FRAME_ID_TT1 , 0);
	tt2_frame = find_frame(tag_data , ID3v2_DOT_3_FRAME_ID_TIT2 , ID3v2_DOT_2_FRAME_ID_TT2 , 0);
	tt3_frame = find_frame(tag_data , ID3v2_DOT_3_FRAME_ID_TIT3 , ID3v2_DOT_2_FRAME_ID_TT3 , 0);

	talb_frame = find_frame(tag_data , ID3v2_DOT_3_FRAME_ID_TALB , ID3v2_DOT_2_FRAME_ID_TAL , 0);

	tcon_frame = find_frame(tag_data , ID3v2_DOT_3_FRAME_ID_TCON , ID3v2_DOT_2_FRAME_ID_TCO , 0);
#if 0
	artist_info_mask = (((tp1_frame != NULL) && ( tp1_frame->fr_data_len > 1))?TP1_BIT:0) |
			(((tp2_frame != NULL) && ( tp2_frame->fr_data_len > 1))?TP2_BIT:0) |
			(((tp3_frame != NULL) && ( tp3_frame->fr_data_len > 1))?TP3_BIT:0) |
			(((tp4_frame != NULL) && ( tp4_frame->fr_data_len > 1))?TP4_BIT:0);

	title_info_mask = (((tt1_frame != NULL) && ( tt1_frame->fr_data_len > 1))?TT1_BIT:0) |
			(((tt2_frame != NULL) && ( tt2_frame->fr_data_len > 1))?TT2_BIT:0) |
			(((tt3_frame != NULL) && ( tt3_frame->fr_data_len > 1))?TT3_BIT:0);
#endif
	if(tp1_frame != NULL)
	{
		tp1_text = get_text_from_frame(tp1_frame);
		artist_info_mask |= ((tp1_text != NULL) && (strlen(tp1_text)>0))?TP1_BIT:0;
	}else {
		tp1_text = NULL;
	}
	if(tp2_frame != NULL)
	{
		tp2_text = get_text_from_frame(tp2_frame);
		artist_info_mask |= ((tp2_text != NULL) && (strlen(tp2_text)>0))?TP2_BIT:0;
	}else {
		tp2_text = NULL;
	}
	if(tp3_frame != NULL)
	{
		tp3_text = get_text_from_frame(tp3_frame);
		artist_info_mask |= ((tp3_text != NULL) && (strlen(tp3_text)>0))?TP3_BIT:0;
	}else {
		tp3_text = NULL;
	}
	if(tp4_frame != NULL)
	{
		tp4_text = get_text_from_frame(tp4_frame);
		artist_info_mask |= ((tp4_text != NULL) && (strlen(tp4_text)>0))?TP4_BIT:0;
	}else {
		tp4_text = NULL;
	}

	if(tt1_frame != NULL)
	{
		tt1_text = get_text_from_frame(tt1_frame);
		title_info_mask |= ((tt1_text != NULL) && (strlen(tt1_text)>0))?TT1_BIT:0;
	}else {
		tt1_text = NULL;
	}
	if(tt2_frame != NULL)
	{
		tt2_text = get_text_from_frame(tt2_frame);
		title_info_mask |= ((tt2_text != NULL) && (strlen(tt2_text)>0))?TT2_BIT:0;
	}else {
		tt2_text = NULL;
	}
	if(tt3_frame != NULL)
	{
		tt3_text = get_text_from_frame(tt3_frame);
		title_info_mask |= ((tt3_text != NULL) && (strlen(tt3_text)>0))?TT3_BIT:0;
	}else {
		tt3_text = NULL;
	}

	if(talb_frame != NULL)
	{
		talb_text = get_text_from_frame(talb_frame);
		if(fields->v2_album == false)
		{
			if((talb_text != NULL) && (strlen(talb_text)>0))
			{
				file_inf->v2_album = talb_text;
				fields->v2_album = true;
			}else if(talb_text != NULL)
			{
				free(talb_text);
			}
		}else {
			if(talb_text != NULL)
			{
				free(talb_text);
			}
		}
	}else {
		talb_text = NULL;
	}

	if(tcon_frame != NULL)
	{
		tcon_text = get_content_from_frame(tcon_frame);
		if(fields->v2_genre == false)
		{
			if((tcon_text != NULL) &&(strlen(tcon_text)>0))
			{
				file_inf->genre = GENRE_MAGIC;
				file_inf->genre_string = file_inf->v2_genre_string = tcon_text;
				fields->v2_genre = true;
			}else if(tcon_text != NULL)
			{
				free(tcon_text);
			}
		}else {
			if(tcon_text != NULL)
			{
				free(tcon_text);
			}
		}
	}else {
		tcon_text = NULL;
	}
	if(fields->v2_artist == false)
	{
	   	fields->v2_artist = true;
	   	switch ( artist_info_mask) {
	   		case 0 :
	   			fields->v2_artist = false;
	   			break;
	   		case TP1_BIT :
	   			file_inf->v2_artist = strdup(tp1_text);
	   			break;
	   		case TP2_BIT :
	   			file_inf->v2_artist = strdup(tp2_text);
	   			break;
	   		case TP3_BIT :
	   			file_inf->v2_artist = strdup(tp3_text);
	   			break;
	   		case TP4_BIT :
	   			file_inf->v2_artist = strdup(tp4_text);
	   			break;

	   		case TP1_BIT | TP2_BIT:
	   			file_inf->v2_artist = (char *)malloc(strlen(tp1_text) + strlen(" - ") + strlen(tp2_text) + 1/* for the null terminating char*/);
	   			if(file_inf->v2_artist != NULL)
	   			{
	   				strcpy(file_inf->v2_artist , tp1_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp2_text);
	   			}
	   			break;
	   		case TP1_BIT | TP3_BIT:
	   			file_inf->v2_artist = (char *)malloc(strlen(tp1_text) + strlen(" - ") + strlen(tp3_text) + 1/* for the null terminating char*/);
	   			if(file_inf->v2_artist != NULL)
	   			{
	   				strcpy(file_inf->v2_artist , tp1_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp3_text);
	   			}
	   			break;
	   		case TP1_BIT | TP4_BIT:
	   			file_inf->v2_artist = (char *)malloc(strlen(tp1_text) + strlen(" - ") + strlen(tp4_text) + 1/* for the null terminating char*/);
	   			if(file_inf->v2_artist != NULL)
	   			{
	   				strcpy(file_inf->v2_artist , tp1_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp4_text);
	   			}
	   			break;

	   		case TP2_BIT | TP3_BIT:
	   			file_inf->v2_artist = (char *)malloc(strlen(tp2_text) + strlen(" - ") + strlen(tp3_text) + 1/* for the null terminating char*/);
	   			if(file_inf->v2_artist != NULL)
	   			{
	   				strcpy(file_inf->v2_artist , tp2_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp3_text);
	   			}
	   			break;
	   		case TP2_BIT | TP4_BIT:
	   			file_inf->v2_artist = (char *)malloc(strlen(tp2_text) + strlen(" - ") + strlen(tp4_text) + 1/* for the null terminating char*/);
	   			if(file_inf->v2_artist != NULL)
	   			{
	   				strcpy(file_inf->v2_artist , tp2_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp4_text);
	   			}
	   			break;

	   		case TP3_BIT | TP4_BIT:
	   			file_inf->v2_artist = (char *)malloc(strlen(tp3_text) + strlen(" - ") + strlen(tp4_text) + 1/* for the null terminating char*/);
	   			if(file_inf->v2_artist != NULL)
	   			{
	   				strcpy(file_inf->v2_artist , tp3_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp4_text);
	   			}
	   			break;


	   		case TP1_BIT | TP2_BIT | TP3_BIT:
	   			file_inf->v2_artist = (char *)malloc(strlen(tp1_text) + strlen(" - ") + strlen(tp2_text) + strlen(" - ") + strlen(tp3_text) + 1/* for the null terminating char*/);
	   			if(file_inf->v2_artist != NULL)
	   			{
	   				strcpy(file_inf->v2_artist , tp1_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp2_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp3_text);
	   			}
	   			break;
	   		case TP1_BIT | TP2_BIT | TP4_BIT:
	   			file_inf->v2_artist = (char *)malloc(strlen(tp1_text) + strlen(" - ") + strlen(tp2_text) + strlen(" - ") + strlen(tp4_text) + 1/* for the null terminating char*/);
	   			if(file_inf->v2_artist != NULL)
	   			{
	   				strcpy(file_inf->v2_artist , tp1_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp2_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp4_text);
	   			}
	   			break;

	   		case TP1_BIT | TP3_BIT | TP4_BIT:
	   			file_inf->v2_artist = (char *)malloc(strlen(tp1_text) + strlen(" - ") + strlen(tp3_text) + strlen(" - ") + strlen(tp4_text) + 1/* for the null terminating char*/);
	   			if(file_inf->v2_artist != NULL)
	   			{
	   				strcpy(file_inf->v2_artist , tp1_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp3_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp4_text);
	   			}
	   			break;
	   		case TP2_BIT | TP3_BIT | TP4_BIT:
	   			file_inf->v2_artist = (char *)malloc(strlen(tp2_text) + strlen(" - ") + strlen(tp3_text) + strlen(" - ") + strlen(tp4_text) + 1/* for the null terminating char*/);
	   			if(file_inf->v2_artist != NULL)
	   			{
	   				strcpy(file_inf->v2_artist , tp2_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp3_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp4_text);
	   			}
	   			break;

	   		case TP1_BIT | TP2_BIT | TP3_BIT | TP4_BIT:
	   			file_inf->v2_artist = (char *)malloc(strlen(tp1_text) + strlen(" - ") + strlen(tp2_text) + strlen(" - ") + strlen(tp3_text) + strlen(" - ") + strlen(tp4_text) + 1/* for the null terminating char*/);
	   			if(file_inf->v2_artist != NULL)
	   			{
	   				strcpy(file_inf->v2_artist , tp1_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp2_text);
	   				strcat(file_inf->v2_artist , " - ");
	   				strcpy(file_inf->v2_artist , tp3_text);
	   				strcat(file_inf->v2_artist , " - ");
					strcpy(file_inf->v2_artist , tp4_text);
				}
				break;
		}
	}
	if(fields->v2_title == false)
	{
		fields->v2_title = true;
		switch ( title_info_mask) {
			case 0 :
				fields->v2_title = false;
				break;
			case TT1_BIT :
				file_inf->v2_title = strdup(tt1_text);
				break;
			case TT2_BIT :
				file_inf->v2_title = strdup(tt2_text);
				break;
			case TT3_BIT :
				file_inf->v2_title = strdup(tt3_text);
				break;

			case TT1_BIT | TT2_BIT:
				file_inf->v2_title = (char *)malloc(strlen(tt1_text) + strlen(" - ") + strlen(tt2_text) + 1/* for the null terminating char*/);
				if(file_inf->v2_title != NULL)
				{
					strcpy(file_inf->v2_title , tt1_text);
					strcat(file_inf->v2_title , " - ");
					strcpy(file_inf->v2_title , tt2_text);
				}
				break;
			case TT1_BIT | TT3_BIT:
				file_inf->v2_title = (char *)malloc(strlen(tt1_text) + strlen(" - ") + strlen(tt3_text) + 1/* for the null terminating char*/);
				if(file_inf->v2_title != NULL)
				{
					strcpy(file_inf->v2_title , tt1_text);
					strcat(file_inf->v2_title , " - ");
					strcpy(file_inf->v2_title , tt3_text);
				}
				break;

			case TT2_BIT | TP3_BIT:
				file_inf->v2_title = (char *)malloc(strlen(tt2_text) + strlen(" - ") + strlen(tt3_text) + 1/* for the null terminating char*/);
				if(file_inf->v2_artist != NULL)
				{
					strcpy(file_inf->v2_title , tt2_text);
					strcat(file_inf->v2_title , " - ");
					strcpy(file_inf->v2_title , tt3_text);
				}
				break;


			case TT1_BIT | TT2_BIT | TT3_BIT:
				file_inf->v2_title = (char *)malloc(strlen(tt1_text) + strlen(" - ") + strlen(tt2_text) + strlen(" - ") + strlen(tt3_text) + 1/* for the null terminating char*/);
				if(file_inf->v2_title != NULL)
				{
					strcpy(file_inf->v2_title , tt1_text);
					strcat(file_inf->v2_title , " - ");
					strcpy(file_inf->v2_title , tt2_text);
					strcat(file_inf->v2_title , " - ");
					strcpy(file_inf->v2_title , tt3_text);
				}
				break;
		}
	}




//	free(tcon_text);
//	free(talb_text);
	free(tp1_text);
	free(tp2_text);
	free(tp3_text);
	free(tp4_text);
	free(tt1_text);
	free(tt2_text);
	free(tt3_text);

}

void free_id3v2_tag_info(id3v2_tag *tag_data)
{
        id3v2frame *current_frame , *next_frame;
        for(current_frame = tag_data->first_frame ; current_frame != NULL ; current_frame = next_frame)
        {
                if(current_frame->fr_data != NULL)
                {
                        free(current_frame->fr_data);
                }
                next_frame = current_frame->next;
                free(current_frame);
        }
}


#if 0
int parse_id3v2_dot_3(mp3_file_info *file_inf , FILE *checked_file  , size_t size ,  id3_fields_found *fields , char *header_buffer , size_t tag_size , id3v2frame **first_frame)
{
	char frame_header_buffer[ID3v2_DOT_3_FRAME_HEADER_SIZE] , *frame_buffer;
	size_t total_read = 0 ;
	size_t frame_size;
	size_t extended_header_length;
	unsigned long int frame_type;
	bool last_frame_valid = true;
	bool unsync_used = false;
	size_t bytes_read;
	size_t real_frame_data_size;
	id3v2frame *new_frame;
	MY_FILE *local_file_handler;
	local_file_handler = my_file_associate_with_file_stream(checked_file);
	while(total_read < tag_size)
	{
		/*-----------------11/12/01 0:06--------------------
		 * TODO: read flags and handle them.
		 * --------------------------------------------------*/
		if(unsyncer_fread(frame_header_buffer , 1 , ID3v2_DOT_3_FRAME_HEADER_SIZE , checked_file , unsync_used , &bytes_read , NULL , false ) != ID3v2_DOT_3_FRAME_HEADER_SIZE)
		{
			return MP3INFO_ECORRUPTEDFILE ;
		}
		total_read += bytes_read;
		frame_type = MAKE_INT_FROM_4_CHARS( frame_header_buffer[0] , frame_header_buffer[1] , frame_header_buffer[2]);
		last_frame_valid = false;
		for(i=0; i<ID3v2_DOT_3_KNOWN_FRAMES_NUMBER ; i++)
		{
			if(known_id3v2_dot_3_frames[i] == frame_type)
			{
				last_frame_valid = true;
				break;
			}
		}
		if(!last_frame_valid)
		{
			/*-----------------10/12/01 22:10-------------------
			 * There may be padding, so we leave now
			 * --------------------------------------------------*/
			break;
		}
		frame_size = (frame_header_buffer[3] << 16) | (frame_header_buffer[4] << 8) | (frame_header_buffer[5]);
		/*-----------------10/12/01 16:59-------------------
		 * Doc is unclear - is the size is for the unsynced data
		 * or for the original data?? I choose the unsynced, according to v2.4
		 * --------------------------------------------------*/
		/*-----------------10/12/01 22:12-------------------
		 * Read the frame data
		 * --------------------------------------------------*/
		frame_buffer = malloc(frame_size);
		if(frame_buffer == NULL)
		{
			return MP3INFO_ENOMEM;
		}

		if(total_read +frame_size > tag_size)
		{
			free(frame_buffer);
			return MP3INFO_ECORRUPTEDFILE ;
		}
		if(unsyncer_fread(frame_buffer , 1 , frame_size , checked_file , unsync_used , &bytes_read , &real_frame_data_size , true ) != frame_size)
		{
			free(frame_buffer);
			return MP3INFO_ECORRUPTEDFILE ;
		}
		total_read += bytes_read;
		if(total_read > tag_size)
		{
			free(frame_buffer);
			return MP3INFO_ECORRUPTEDFILE ;
		}
		/*-----------------10/12/01 22:08-------------------
		 * build a frame block, and add it to the list
		 * --------------------------------------------------*/
		new_frame = new id3v2frame;
		if(new_frame == NULL)
		{
			free(frame_buffer);
			return MP3INFO_ENOMEM;
		}
		new_frame->fr_data = frame_buffer;
		new_frame->fr_data_len = real_frame_data_size;
		new_frame->fr_data_z = NULL;    /* ID3v2.2 had no frame compression */
		new_frame->fr_data_z_len = 0;
		new_frame->id3v2_major_version = 0x02;
		new_frame->frame_id = frame_type;
		new_frame->next = *first_frame;
		new_frame->previous = NULL;
		if(*first_frame != NULL)
		{
		   (*first_frame)->previous = new_frame;
		}
		*first_frame = new_frame;
	}
	return ID3V2TAG_OK;
}
#endif


int read_id3v2(mp3_file_info *file_inf , FILE *checked_file  , long int size ,  id3_fields_found *fields)
{
	long int starting_point;
	int parsers_return_value;
	bool no_id3v2 = false;
	bool unsync_used = false;
	unsigned char id3v2buffer[ID3V2_MAIN_HEADER_SIZE];
        char extended_header_buffer[4];
        size_t bytes_read , extended_header_length;
	id3v2_tag *the_v2_tag;
	size_t id3v2tagsize;
	MY_FILE *local_file_handler;
	the_v2_tag = new id3v2_tag;
	if(the_v2_tag == NULL)
	{
		return MP3INFO_ENOMEM;
	}
        memset(the_v2_tag , 0 , sizeof(id3v2_tag));
	local_file_handler = my_file_associate_with_file_stream(checked_file);
	starting_point = my_fseek(local_file_handler,0,SEEK_CUR);
	if(my_fread(id3v2buffer , 1 , ID3V2_MAIN_HEADER_SIZE , local_file_handler) != ID3V2_MAIN_HEADER_SIZE)
	{
		no_id3v2 = true;
		goto after_id3v2_search;
	}
	if(strncmp((const char *)id3v2buffer , ID3V2_HEADER_MAGIC , strlen(ID3V2_HEADER_MAGIC))!= 0)
	{
		no_id3v2 = true;
		goto after_id3v2_search;
	}
	if((id3v2buffer[3] > 0xFE)||(id3v2buffer[4] > 0xFE)/* Version*/ ||
			(id3v2buffer[6] > 0x7F) ||(id3v2buffer[7] > 0x7F) ||(id3v2buffer[8] > 0x7F) ||(id3v2buffer[9] > 0x7F) /* Tag Length*/)
	{
		no_id3v2 = true;
		goto after_id3v2_search;
	}
	id3v2tagsize = (id3v2buffer[6] << 21) | (id3v2buffer[7] << 14) | (id3v2buffer[8] << 7) | (id3v2buffer[9]);
	if(id3v2tagsize+starting_point+ID3V2_MAIN_HEADER_SIZE > size)
	{
		no_id3v2 = true;
		goto after_id3v2_search;
	}
	if(id3v2buffer[5] & 0x80)
	{
		unsync_used = true;
	}
	switch ( id3v2buffer[3] /* Version*/) {
		case 0x02 :
			if(id3v2buffer[5] & 0x40)
			{
				if(id3v2buffer[4]>0)
				{
					/*-----------------11/12/01 0:06--------------------
					 * Skip extended header
					 * --------------------------------------------------*/
					if(unsyncer_fread(extended_header_buffer , 1 , 4 , local_file_handler , unsync_used , &bytes_read , NULL , false ) != 4)  /* MAGIC! MAGIC! */
					{
						no_id3v2 = true;
						goto after_id3v2_search;
					}
//					total_read += bytes_read;
					extended_header_length  = ((extended_header_buffer[0]<<24)|(extended_header_buffer[1]<<16)|(extended_header_buffer[2]<<8)|extended_header_buffer[3]);
					fseek(checked_file , extended_header_length , SEEK_CUR);
				}else {
					goto after_id3v2_search;
				}
			}
//			parsers_return_value = parse_id3v2_dot_2(file_inf , checked_file , size , fields , unsync_used , id3v2tagsize , &first_frame );
			break;
		case 0x03 :
			if(id3v2buffer[5] & 0x40)
			{
				/*-----------------11/12/01 0:06--------------------
				 * Skip extended header
				 * --------------------------------------------------*/
				if(unsyncer_fread(extended_header_buffer , 1 , 4 , local_file_handler , unsync_used , &bytes_read , NULL , false ) != 4)  /* MAGIC! MAGIC! */
				{
					no_id3v2 = true;
					goto after_id3v2_search;
				}
//				total_read += bytes_read;
				extended_header_length  = ((extended_header_buffer[0]<<24)|(extended_header_buffer[1]<<16)|(extended_header_buffer[2]<<8)|extended_header_buffer[3]);
				fseek(checked_file , extended_header_length , SEEK_CUR);
			}
//			parsers_return_value = parse_id3v2_dot_3(file_inf , checked_file , size , fields , unsync_used , id3v2tagsize , &first_frame );
			break;
		default:
			goto after_id3v2_search;    /* Unknown version of ID3v2. Skip it. */
//			break;
	}
	parsers_return_value = parse_id3v2( local_file_handler , size ,  unsync_used , id3v2tagsize , the_v2_tag , id3v2buffer[3] );
	switch ( parsers_return_value) {
		case MP3INFO_ECORRUPTEDFILE :
			no_id3v2 = true;
		case 0 :
			break;
		default:
			my_file_free(local_file_handler);
			return parsers_return_value;
	}
//	if(!no_id3v2)
//	{
//		read_
	copy_id3v2_into_file_info(the_v2_tag , file_inf , fields);
        //dump_id3v2_tag(the_v2_tag);
        free_id3v2_tag_info(the_v2_tag);
after_id3v2_search:
        delete the_v2_tag;
	if(!no_id3v2)
	{
		starting_point += id3v2tagsize+ID3V2_MAIN_HEADER_SIZE;
		file_inf->id3v2found = true;
	}
else	{
		file_inf->id3v2found = false;
	}
	my_fseek(local_file_handler , starting_point , SEEK_SET);
        my_file_free(local_file_handler);
	return 0;
}




