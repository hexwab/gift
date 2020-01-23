#include "meta.h"
#include <stdlib.h>
#include <stdio.h>

int 
main (void)
{
	List *md_list;
	for (;;)
	{
		md_list = NULL;
		if (!add_tag_string (&md_list, TAG_MIME, strdup ("audio/mpeg")))
			perror ("add_tag_string");
		
		if (!add_tag_string (&md_list, TAG_REALM, strdup ("Document")))
			perror ("add_tag_string");
		
		
		{
			unsigned char *s;
			//  dump_tags(md_list);
			printf ("to_integer test: %lu\n", tag_to_integer (md_list->data));
			printf ("to_string test: '%s'\n", s = tag_to_string (md_list->data));
			free (s);
			printf ("to_integer test: %lu\n", tag_to_integer (list_next (md_list)->data));
			printf ("to_string test: '%s'\n", s = tag_to_string (list_next (md_list)->data));
			free (s);
		}
		free_metadata (md_list);
	}
	return 0;
}