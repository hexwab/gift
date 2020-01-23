#ifndef _META_H_

#define META_INTEGER 0
#define META_STRING  1
#define META_ENUM    2

#define META_MASK    3

#define META_SHIFT   2

#define TAG_REALM    0x0c
#define TAG_BITRATE  0x10
#define TAG_DURATION 0x14
#define TAG_WIDTH    0x18
#define TAG_HEIGHT   0x1c
#define TAG_TITLE    0x20
#define TAG_ARTIST   0x24
#define TAG_ALBUM    0x28
#define TAG_COMMENT  0x2c
#define TAG_YEAR     0x30
#define TAG_TRACK    0x34
#define TAG_MIME     0x38
#define TAG_CHANNELS 0x3c
#define TAG_FREQ     0x40

#include "list.h"

typedef
struct metadata
{
	int type;
	union
	{
		unsigned long int_val;	/* should be long long? */
		unsigned char *str_val;
		unsigned long enum_val;
	}
	value;
}
Metadata;


Metadata * meta_add_integer (List ** md_list, int tag, unsigned long value);
Metadata * meta_add_string (List ** md_list, int tag, unsigned char *value);
Metadata * meta_add_enum (List ** md_list, int tag, unsigned long value);
unsigned long meta_get_integer (Metadata * md);
unsigned char *meta_get_string (Metadata * md);
void meta_free (List * md_list);
Metadata * meta_lookup (List *md_list, int tag, List **next_ptr);


#define _META_H_
#endif
