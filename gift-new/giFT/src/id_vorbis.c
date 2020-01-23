#include "id.h"
#include "meta.h"

#define READ(ptr,size) if (!fread (ptr, size, 1, fh)) return 0;

#define WORD_LE(ptr) ((ptr)[0]+((ptr)[1]<<8)+((ptr)[2]<<16)+((ptr)[3]<<24))

static FILE *fh;
static int packets, packet, pcount;
static int split_packet;
static int csize;
static int freq;
static int packet_len[255];

static struct
{
	int tag;
	char *name;
}
vorbis_tags[]=
{
	{ TAG_ARTIST, "ARTIST" },
	{ TAG_TITLE, "TITLE" },
	{ TAG_ALBUM, "ALBUM" },
	{ TAG_TRACK, "TRACKNUMBER" }
};

#define NUM_VORBIS_TAGS  4

static int read_page (void)
{
	unsigned char ph[27], segtable[255];
	int i, nsegs;
     
	READ (ph,27);
	if (strcmp (ph,"OggS"))
		return 0;
	nsegs = ph[26];
	packets = 0; /* FIXME */
	packet = 0;
	READ (segtable,nsegs);
	for(i=0; i<nsegs; i++)
	{
		csize += segtable[i];
		split_packet = (segtable[i]==255);
		if (!split_packet)
		{
/*	       printf("packet: %d\n",csize); */
			packet_len[packets++]=csize;
			csize=0;
		}
	}
	return 1;
}

static int ogg_read (char *buf, int size)
{
	while (size)
	{
		if (!packets && !read_page ())
			return 0;
		if (size<=packet_len[packet])
		{
			READ (buf, size);
			if (!(packet_len[packet]-=size))
			{
				packet++;
				packets--;
				pcount++;
			}
			size=0;
		}
		else
			return 0;
	}
	return 1;
}

static int skip_packet (void)
{
	if (!packets && !read_page ())
		return 0;
	if (fseek (fh,packet_len[packet],SEEK_CUR))
		return 0;
	packets--;
	packet++;
	pcount++;
	return 1;
}

static int read_duration (List **md_list)
{
	unsigned char *buf, *ptr;
	int len;
	int flag=0;

	unsigned long long duration;

	if (freq<0)
		return 0;

	fseek (fh,0, SEEK_END);
	len = ftell(fh);
	if (len>65307)
		len=65307; /* maximum page size */
	if (len<27)
		return 0;
	fseek (fh,-len,SEEK_END);
	buf = malloc (len);
	if (!buf)
		return 0;

	if (fread (buf, len, 1, fh))
	{
		/* TODO: optimize this */
		for(ptr=buf+len-27;ptr>=buf;ptr--)
		{
			if (!strcmp (ptr, "OggS"))
			{
#if 0
				if (!(ptr[5] & 4))
					printf("stream is possibly incomplete\n");
#endif

				duration=WORD_LE(ptr+6)+((unsigned long long)(WORD_LE(ptr+10))<<32);
				(void) meta_add_integer (md_list, TAG_DURATION, (unsigned long)(duration/freq));
				flag=1;
				break;
			}
		}
	}
	free (buf);
	return flag;
}

int id_vorbis (FILE *f, List **md_list)
{
	unsigned char buf[256];
	fh = f;
	split_packet = 0;
	packets = 0;
	pcount = 0;
	csize = 0;
	freq = -1;
	for(;;)
	{
		if (!ogg_read (buf, 7))
			return 0;
	  
		if (strncmp (buf+1,"vorbis",6))
/*	       printf ("mismatch: %s\n", buf+1); */
			return 0;
		switch (*buf)
		{
		case 1:
			if (!ogg_read (buf, 21))
				return 0;
			freq=WORD_LE (buf+5);

			(void) meta_add_integer (md_list,TAG_CHANNELS,buf[4]);
			(void) meta_add_integer (md_list, TAG_FREQ, freq);
			(void) meta_add_integer (md_list,TAG_BITRATE,WORD_LE(buf+13)/1000);
			break;
		case 3:
		{
			unsigned char *str, *val;
			int len, comments;
			if (!ogg_read (buf, 4))
				return 0;
			len=WORD_LE(buf);
			if (!(str=malloc (len+1)))
				return 0;
			if (!ogg_read (str, len))
			{
				free (str);
				return 0;
			}
			str[len]=0;
			/*printf ("ver: %s\n",str);*/
			if (!ogg_read (buf, 4))
				return 0;
			for(comments=WORD_LE (buf);comments;comments--)
			{
				int tag;
				if (!ogg_read (buf, 4))
					return 0;
				len=WORD_LE(buf);
				if (!(str=malloc (len+1)))
					return 0;
				if (!ogg_read (str, len))
				{
					free (str);
					return 0;
				}
				str[len] = 0;
				val = strchr (str,'=');
				if (!val)
				{
					free(str);
					continue;
				}
				*val++=0;

				/* FIXME: utf-8 conversion */
				for(tag=0;tag<NUM_VORBIS_TAGS;tag++)
				{
					if (!strcasecmp (str,vorbis_tags[tag].name))
					{
						(void) meta_add_string (md_list, vorbis_tags[tag].tag, strdup (val));
						break;
					}
				}

				free(str);
			}
	       
			break;
		}
		default:
			TRACE(("unknown: %d", *buf));
		}
		if (pcount)
			break;
		skip_packet ();
	}
	(void) meta_add_string (md_list, TAG_MIME, strdup ("application/x-ogg"));
	return read_duration (md_list);
}
