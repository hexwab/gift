struct _bittable {
	uint8_t  *table;
	int       size; /* in bits */
}

typedef struct _bittable BitTable;

BitTable *ft_bittable_new (int size)
{
	BitTable *bt;

	/* make sure it's a reasonably-sized power of two */
	if (size < 8 || (size & (size-1) != 0))
		return NULL;
	
	bt = MALLOC (sizeof(BitTable));
	
	if (!bt)
		return NULL;
	
	bt->table = CALLOC (size >> 3);

	if (!bt->table)
	{
		free (bt);
		return NULL;
	}

	bt->size = size;
	
	return bt;
}

void ft_bittable_set (BitTable *bt, int bit)
{
	int offset = bit & (bt->size-1);
	
	bt->table[offset >> 3] |= 1 << (offset & 7);
}

void ft_bittable_unset (BitTable *bt, int bit)
{
	int offset = bit & (bt->size-1);
	
	bt->table[offset >> 3] &= ~(1 << (offset & 7));
}

void ft_bittable_clear (BitTable *bt)
{
	memset (bt->table, 0, bt->size);
}

