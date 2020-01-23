#include "enum.h"
#include "gift.h"

const unsigned char *enum_mime[] =
{
	"application/octet-stream",
	"text/plain",
	"image/gif",
	"audio/mpeg",
	"video/mpeg",
	"image/jpeg",
	"application/x-msvideo",
	"application/x-ogg"
};
#define MIME_SIZE 8

const unsigned char *enum_realm[] =
{
	"Unknown",
	"Audio",
	"Video",
	"Image",
	"Document",
	"Other",
};
#define REALM_SIZE 5

typedef struct
{
	const char *name;
	const int size;
	const unsigned char **list;
}
Enum_list;

Enum_list tag_enums[] =
{
	{"name", 0, NULL},		/* filename (unused) */
	{"size", 0, NULL},		/* filesize */
	{"md5",  0, NULL},		/* md5 */
	{"realm", REALM_SIZE, enum_realm},	/* realm */
	{"bitrate", 0, NULL},		/* bitrate/kbps */
	{"duration", 0, NULL},		/* duration/sec */
	{"width", 0, NULL},		/* Xres/px */
	{"height", 0, NULL},		/* Yres/px */
	{"title", 0, NULL},		/* title */
	{"artist", 0, NULL},		/* artist */
	{"album", 0, NULL},		/* album */
	{"comment", 0, NULL},		/* comment */
	{"year", 0, NULL},		/* year */
	{"track", 0, NULL},		/* track number */
	{"type", MIME_SIZE, enum_mime},	/* MIME type */
	{"channels", 0, NULL},          /* channels */
	{"freq", 0, NULL},              /* sampling frequency/Hz */
};

#define listsize(l) (sizeof(l)/sizeof(l[0]))

#define TAG_ENUMS_SIZE listsize(tag_enums)


unsigned char *meta_decode_enum (Metadata * md)
{
	unsigned char *string = NULL;
	Enum_list *el;
	unsigned int type;
	if ((md->type & META_MASK) != META_ENUM)
	{
		GIFT_WARN (("non-enum tag 0x%x\n", md->type));
		return NULL;
	}
	type = (md->type >> META_SHIFT);
	if (type <= TAG_ENUMS_SIZE)
	{
		el = tag_enums + type;
		if (md->value.enum_val < el->size)
			string = el->list[md->value.enum_val];
		else
			GIFT_WARN (("value %lu out of range (max %u)\n", md->value.enum_val, el->size));
		
	}
	return string;
}

Metadata *meta_encode_enum (Metadata * md)
{
	Enum_list *el;
	unsigned int type;
	if (!md)
		return NULL;
	if ((md->type & META_MASK) != META_STRING)
		return md;

	type = (md->type >> META_SHIFT);
	if (type <= TAG_ENUMS_SIZE)
	{
		const unsigned char **ptr;
		int count = 0;
		el = tag_enums + type;
		ptr = el->list;
		while (count < el->size)
		{
			if (!strcmp (*ptr, md->value.str_val))
				break;
			ptr++;
			count++;
		}
		if (count != el->size)
		{
			free (md->value.str_val);
			md->type = (md->type & ~META_MASK) | META_ENUM;
			md->value.enum_val = count;
			/*GIFT_DEBUG (("is an enum! type %d val %d", md->type, count));*/
		}
	}
	return md;
}

const char *meta_tag_name (unsigned int type)
{
	const char *name=NULL;
	type >>= META_SHIFT;
	if (type <= TAG_ENUMS_SIZE)
		name=tag_enums[type].name;
	return name;
}
