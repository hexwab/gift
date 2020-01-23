/* identify a file and return some metadata */

#include "id.h"
#include "id_vorbis.h"
#include "id_jpeg.h"
#include "meta.h"
#include "mime.h"

#define NUM_ID_FUNCS 2

static IdFunc id_func[]=
{
	id_vorbis,
	id_jpeg
};

List *id_file (char *path)
{
	FILE *fh;
	List *md_list=NULL;
	int i;
	if ((fh=fopen (path, "rb")))
	{	
		for(i=0;i<NUM_ID_FUNCS;i++)
		{
			fseek(fh, 0, SEEK_SET);
			if ((id_func[i]) (fh,&md_list))
			{
#if 0
				TRACE(("%s",path));

				TRACE(("title lookup: %s", tag_to_string(tag_lookup(md_list,TAG_TITLE,NULL))));
				TRACE(("MIME lookup: %s", tag_to_string(tag_lookup(md_list,TAG_,NULL))));
//				meta_dump(md_list);
#endif
				break;
			}

			/* remove any partial data we may have got before giving up */
			if (md_list)
				meta_free (md_list);
			md_list=NULL;
		}
		fclose (fh);
	}

	/* try and guess a MIME type if all else fails */
	if (!md_list)
	{
		char *mime=mime_type (path, NULL);
		if (mime)
			(void) meta_add_string (&md_list, TAG_MIME, strdup (mime));
	}

	return md_list;
}
