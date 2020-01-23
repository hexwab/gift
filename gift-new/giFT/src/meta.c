#include "gift.h"
#include "meta.h"
#include "list.h"
#include "enum.h"

static Metadata *new_tag (List ** md_list)
{
	Metadata *md = malloc (sizeof (Metadata));
	if (!md)
	{
		TRACE(("malloc failed!"));
		return 0;
	}
	//  printf("new_tag: %p\n",md);
	*md_list = list_append (*md_list, md);
	return md;
}

Metadata *meta_add_integer (List ** md_list, int tag, unsigned long value)
{
	Metadata *md = new_tag (md_list);
	if (!md)
		return 0;
	md->type = (tag & ~META_MASK) | META_INTEGER;
	md->value.int_val = value;
	return md;
}

Metadata *meta_add_string (List ** md_list, int tag, unsigned char *value)
{
	Metadata *md = new_tag (md_list);
	if (!md)
		return 0;
	md->type = (tag & ~META_MASK) | META_STRING;
	md->value.str_val = value;
	meta_encode_enum (md);
	return md;
}

Metadata *meta_add_enum (List ** md_list, int tag, unsigned long value)
{
	Metadata *md = new_tag (md_list);
	if (!md)
		return 0;
	md->type = (tag & ~META_MASK) | META_ENUM;
	md->value.enum_val = value;
	return md;
}

static int dump_tags_callback (void *data, void *unused)
{
	Metadata *md = data;
	const char *t=meta_tag_name(md->type);
	switch (md->type & META_MASK)
	{
	case META_INTEGER:
		TRACE (("%s: %lu",t, md->value.int_val));
		break;
	case META_STRING:
		TRACE (("%s: %s",t, md->value.str_val));
		break;
	case META_ENUM:
		TRACE (("%s: %lu [enum]",t, md->value.enum_val));
		break;
	}
	return 0;
}

void meta_dump (List * md_list)
{
	list_foreach (md_list, dump_tags_callback, NULL);
}

static int meta_free_callback (void *data, void *unused)
{
	Metadata *md = data;
	//  GIFT_DEBUG (("0x%x, %p\n",md->type, md));
	if ((md->type & META_MASK) == META_STRING)
		free (md->value.str_val);
	free (data);
	return 0;
}

void meta_free (List * md_list)
{
	list_foreach (md_list, meta_free_callback, NULL);
	list_free (md_list);
}

unsigned long meta_get_integer (Metadata * md)
{
	unsigned char *string;
	unsigned long value = 0;

	switch (md->type & META_MASK)
	{
	case META_INTEGER:
		value = md->value.int_val;
		break;
	case META_STRING:
		value = strtoul (md->value.str_val, NULL, 0);
		break;
	case META_ENUM:
		string = meta_decode_enum (md);
		if (string)
			value = strtoul (string, NULL, 0);
		break;
	default:
		GIFT_WARN (("Unknown meta tag type 0x%x\n", md->type));
		break;
	}
	return value;
}

unsigned char * meta_get_string (Metadata * md)
{
	unsigned char *string = NULL;
	if (md)
	{
		switch (md->type & META_MASK)
		{
		case META_INTEGER:
			/* XXX should do some funky sprintf stuff here */
			break;
		case META_STRING:
			string = md->value.str_val;
			break;
		case META_ENUM:
			string = meta_decode_enum (md);
			break;
		default:
			GIFT_WARN (("unknown tag type 0x%x\n", md->type));
		}
	}
	return string;
}

Metadata * meta_lookup (List *md_list, int tag, List **next_ptr)
{
	for (; md_list; md_list = list_next (md_list))
	{
		Metadata *md=(Metadata *)md_list->data;
		if (md && ((md->type & ~META_MASK) == tag))
		{
			/*TRACE(("%d",tag));*/
			if (next_ptr)
				*next_ptr = list_next (md_list);
			return md;
		}
	}
	return NULL;
}
