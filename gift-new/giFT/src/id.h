#include "gift.h"
#include "list.h"

typedef int (*IdFunc) (FILE *fh, List **md_list);

List *id_file(char *path);
