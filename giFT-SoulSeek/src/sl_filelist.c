/*
 * Copyright (C) 2003 Felix Nawothnig (felix.nawothnig@t-online.de)
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

#include "sl_soulseek.h"
#include "sl_meta.h"
#include "sl_packet.h"
#include "sl_utils.h"

#include "sl_filelist.h"

#include "libgift/mime.h"

List *sl_filelist_new_from_packet(SLPacket *packet)
{
	List *list = NULL;

	int      error;
	uint32_t files = sl_packet_get_integer(packet, &error);

	if(error == TRUE)
		return NULL;

	while(files--)
	{
		int i;
	
		SLFile *file = NEW(SLFile);

		sl_packet_get_byte(packet, &error); /* FIXME: "code"? whats this? Always 1? */
		
		file->path  = sl_packet_get_string(packet);
		file->size  = sl_packet_get_integer(packet, &error);
		
		sl_packet_get_integer(packet, &error); /* FIXME: I guess this is a uint64? */
		
		if(sl_packet_get_string(packet) == NULL)
			error = TRUE;
		
		file->num_attributes = sl_packet_get_integer(packet, &error);
		file->attributes     = MALLOC(file->num_attributes * sizeof(SLMetaAttribute));
		
		for(i = 0; i < file->num_attributes; i++)
		{
			file->attributes[i].type  = sl_packet_get_integer(packet, &error);
			file->attributes[i].value = sl_packet_get_integer(packet, &error);
		}
		
		sl_switch_forward_slashes(file->path);

		if(file->path == NULL)
			error = TRUE;

		assert(error == FALSE); /* FIXME */

		list = list_append(list, file);
	}

	return list;
}

static void report_file(SLFile *file, void **args)
{
	struct file_share  share;
	struct if_event   *event;
	
	SLPeer *peer;
	char   *file_url;
	int     i;

	event = args[0];
	peer  = args[1];

	share_init    (&share, sl_string_to_cstr(file->path));
	share_set_mime(&share, mime_type(share.path));

	share.size = file->size;

	for(i = 0; i < file->num_attributes; i++)
	{
		char buf[64];
		sprintf(buf, "%d", file->attributes[i].value);

		switch(file->attributes[i].type)
		{
			case META_ATTRIBUTE_BITRATE:
				share_set_meta(&share, "bitrate", buf);
				break;
			case META_ATTRIBUTE_LENGTH:
				share_set_meta(&share, "length", buf);
				break;
			default:
				SL_PROTO->dbg(SL_PROTO,
				              "Got unknown meta attribute (0x%.2x)",
					      file->attributes[i].type);
				break;
		}
	}

	file_url = stringf_dup("slsk://%s/%s", sl_string_to_cstr(peer->username),
	                       sl_string_to_cstr(file->path));

	SL_PROTO->search_result(SL_PROTO, event, sl_string_to_cstr(peer->username),
	                        "SoulSeek", file_url, peer->free_slots, &share);

	share_finish(&share);
}

void sl_filelist_report(List *list, struct if_event *event, SLPeer *peer)
{
	void *args[3];

	args[0] = event;
	args[1] = peer;
	args[2] = NULL;

	list_foreach(list, (ListForeachFunc)report_file, args);
}

