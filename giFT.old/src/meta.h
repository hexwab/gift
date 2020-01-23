#ifndef _META_H_

#define META_INTEGER 0
#define META_STRING  1
#define META_ENUM    2

#define META_MASK    3

#define META_SHIFT   2

#define TAG_BITRATE  0x10
#define TAG_DURATION 0x14
#define TAG_REALM    0x0c
#define TAG_MIME     0x38


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


Metadata * add_tag_integer (List ** md_list, int tag, unsigned long value);
Metadata * add_tag_string (List ** md_list, int tag, unsigned char *value);
Metadata * add_tag_enum (List ** md_list, int tag, unsigned long value);
unsigned long tag_to_integer (Metadata * md);
unsigned char *tag_to_string (Metadata * md);
void free_metadata (List * md_list);


#define _META_H_
#endif
