#include "gift.h"
#include "meta.h"
#include "list.h"
#include "enum.h"

static Metadata *
new_tag (List ** md_list)
{
	Metadata *md = malloc (sizeof (*md));
	if (!md)
	{
		TRACE(("malloc failed!"));
		return 0;
	}
	//  printf("new_tag: %p\n",md);
	*md_list = list_append (*md_list, md);
	return md;
}

Metadata *
add_tag_integer (List ** md_list, int tag, unsigned long value)
{
	Metadata *md = new_tag (md_list);
	if (!md)
		return 0;
	md->type = (tag & ~META_MASK) | META_INTEGER;
	md->value.int_val = value;
	return md;
}

Metadata *
add_tag_string (List ** md_list, int tag, unsigned char *value)
{
	Metadata *md = new_tag (md_list);
	if (!md)
		return 0;
	md->type = (tag & ~META_MASK) | META_STRING;
	md->value.str_val = value;
	encode_enum (md);
	return md;
}

Metadata *
add_tag_enum (List ** md_list, int tag, unsigned long value)
{
	Metadata *md = new_tag (md_list);
	if (!md)
		return 0;
	md->type = (tag & ~META_MASK) | META_ENUM;
	md->value.enum_val = value;
	return md;
}

#if 0
static int 
dump_tags_callback (void *data, void *unused)
{
	Metadata *md = data;
	printf ("tag 0x%02x\n", md->type);
	switch (md->type & META_MASK)
	{
	case META_INTEGER:
		printf ("value: %lu\n", md->value.int_val);
		break;
	case META_STRING:
		printf ("value: %s\n", md->value.str_val);
		break;
	}
	return 0;
}

void 
dump_tags (List * md_list)
{
	list_foreach (md_list, dump_tags_callback, NULL);
}
#endif

static int 
free_metadata_callback (void *data, void *unused)
{
	Metadata *md = data;
	//  GIFT_DEBUG (("0x%x, %p\n",md->type, md));
	if ((md->type & META_MASK) == META_STRING)
		free (md->value.str_val);
	free (data);
	return 0;
}

void 
free_metadata (List * md_list)
{
	list_foreach (md_list, free_metadata_callback, NULL);
	list_free (md_list);
}

unsigned long 
tag_to_integer (Metadata * md)
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
		string = decode_enum (md);
		if (string)
		{
			value = strtoul (string, NULL, 0);
			free (string);
		}
		break;
	default:
		GIFT_WARN (("Unknown meta tag type 0x%x\n", md->type));
		break;
	}
	return value;
}

unsigned char *
tag_to_string (Metadata * md)
{
	unsigned char *string = NULL;
	switch (md->type & META_MASK)
	{
	case META_INTEGER:
		/* XXX should do some funky sprintf stuff here */
		break;
	case META_STRING:
		string = strdup (md->value.str_val);
		break;
	case META_ENUM:
		string = decode_enum (md);
		break;
	default:
		GIFT_WARN (("unknown tag type 0x%x\n", md->type));
	}
	return string;
}

#if 0
unsigned char *
pack_metadata_list (List * md_list, unsigned char *buf, unsigned char *end)
{

}

unsigned char *
pack_metadata (Metadata * md, unsigned char *buf, unsigned char *end)
{
	buf = pack_integer (md->type, buf, end);
	switch (md->type & META_MASK)
	{
	case META_INTEGER:
		buf = pack_integer (md->value.int_val, end);
		break;
	case META_ENUM:
		buf = pack_integer (md->value.enum_val, end);
		break;
	case META_STRING:
	{
		int len = strlen (md->value.str_val);
		
		break;
	}
	}
}

unsigned char *
pack_integer (unsigned long value, unsigned char *buf, unsigned char *end)
{
	unsigned long i=value;
	unsigned char *ptr=buf, *foo;
	while (i)
	{
		ptr++;
		i>>=7;
	}
	if (ptr>end)
		return NULL;
	foo=ptr;

	while (value)
	{
		*(--ptr)=value &0x7f;
		if (value>127)
			*ptr|=0x80;
	}
	return foo;
}
#endif