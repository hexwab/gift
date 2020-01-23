#include "ft_openft.h"
#include <ctype.h>

/*
 * Defines a special set of characters which will be skipped when tokenizing
 * each individual word.  For example, "foo!bar" would produce the same
 * token as "foobar".
 */
#define SEARCH_TOKEN_PUNCT ",`'!?*"

/*
 * Defines the set of characters which will be used a delimiters between
 * individual token words.  In addition to this set, numbers are treated
 * as special delimiters through a second string scan.  For example,
 * "s03e21" will produce the token word "s03e21", "3", and "21".
 */
#define SEARCH_TOKEN_DELIM "\\/ _-.[]()\t"

/*****************************************************************************/


static BOOL is_token_punct (int c)
{
	const char *ptr;

	/* TODO: Lots of room for optimization here */
#if 0
	return BOOL_EXPR(strchr(SEARCH_TOKEN_PUNCT, c));
#else
	for (ptr = SEARCH_TOKEN_PUNCT; *ptr != '\0'; ptr++)
	{
		if (*ptr == c)
			return TRUE;
	}
	return FALSE;
#endif
}

static int next_letter (const char **strref, size_t *lenref)
{
	const char *str = *strref;
	size_t len = *lenref;
	int c;

	if (len == 0)
		return 0;

	/* skip any punctuation characters while scanning the word so that we
	 * don't need to actually modify the word */
	while (is_token_punct (*str) == TRUE)
	{
		if (len == 1)
			return 0;

		str++;
		len--;
	}

	c = tolower (*str);
	assert (c != '\0');

	*strref = str + 1;
	*lenref = len - 1;

	return c;
}

/*
 * I believe this came from an old version of GLib or something.  Verify
 * later and add the appropriate credit.
 */
static uint32_t make_token (const char *word, size_t len)
{
	uint32_t hash = 0;
	int letter;

	if (word == NULL)
		return 0;

	if ((letter = next_letter (&word, &len)) == 0)
		return 0;

	hash = letter;

	while ((letter = next_letter (&word, &len)) != 0)
		hash = (hash << 5) - hash + letter;

	return hash;
}

/*****************************************************************************/

struct token_list
{
	uint32_t *tokens;
	size_t    nmemb;
	size_t    size;
	uint8_t  *order;
	
};

static void tlist_init (struct token_list *tlist)
{
	tlist->tokens = NULL;
	tlist->nmemb = 0;
	tlist->size = 0;
}

static BOOL tlist_resize_min (struct token_list *tlist, size_t nmemb)
{
	uint32_t *newalloc;
	size_t size;

	if (tlist->size >= nmemb)
		return TRUE;

	if ((size = tlist->size) == 0)
		size = 1;

	while (size < nmemb)
		size *= 2;

	if (!(newalloc = realloc (tlist->tokens, size * sizeof (uint32_t))))
		return FALSE;

	tlist->tokens = newalloc;
	tlist->size = size;

	return TRUE;
}

static BOOL tlist_add (struct token_list *tlist, uint32_t token)
{
	if (tlist_resize_min (tlist, tlist->nmemb + 1) == FALSE)
		return FALSE;

	tlist->tokens[tlist->nmemb++] = token;

	return TRUE;
}

static BOOL tlist_addword (struct token_list *tlist,
                           const char *word, size_t wordlen)
{
	uint32_t token;

	if ((token = make_token (word, wordlen)) > 0)
		return tlist_add (tlist, token);

	return FALSE;
}

static void add_numbers (struct token_list *tlist, const char *str)
{
	char *ptr;
	size_t numlen;

	/* implicitly scan past leading 0's as they are found only for padding,
	 * not to represent octal numbers */
	while ((ptr = strpbrk (str, "123456789")))
	{
		numlen = strspn (ptr, "0123456789");
		assert (numlen > 0);

		tlist_addword (tlist, ptr, numlen);

		str = ptr + numlen;
	}
}

static void add_words (struct token_list *tlist, const char *str)
{
	size_t wordlen;

	while (1)
	{
		if ((wordlen = strcspn (str, SEARCH_TOKEN_DELIM)) > 0)
			tlist_addword (tlist, str, wordlen);

		if (str[wordlen] == '\0')
			break;

		str += wordlen + 1;
	}
}

static void tlist_addstr (struct token_list *tlist, const char *str)
{
	/* lets us be lazy in the usage */
	if (str == NULL)
		return;

	add_numbers (tlist, str);
	add_words (tlist, str);
}

static int token_cmp (const void *a, const void *b)
{
	return INTCMP (*(uint32_t *)a, *(uint32_t *)b);
}

static void sort_and_uniq (struct token_list *tlist)
{
	size_t i;
	size_t nmemb = 0;
	uint32_t lasttoken = 0;

	if (tlist->nmemb == 0)
		return;

	/* sort */
	qsort (tlist->tokens, tlist->nmemb, sizeof (uint32_t), token_cmp);

	/* ... and uniq */
	for (i = 0; i < tlist->nmemb; i++)
	{
		if (lasttoken > 0)
		{
			/* skip duplicates */
			if (tlist->tokens[i] == lasttoken)
				continue;
		}

		lasttoken = tlist->tokens[i];
		assert (lasttoken != 0);

		/*
		 * Only update the token list position if we have actually detected a
		 * duplicate and the number of elements in the new list will differ
		 * from the old list.
		 */
		if (nmemb != i)
			tlist->tokens[nmemb] = lasttoken;

		nmemb++;
	}

	tlist->nmemb = nmemb;
}

static uint32_t *tlist_finish (struct token_list *tlist)
{
  //	printf("len=%d\n", tlist->nmemb);
	/* sort the token list, then remove duplicates (by way of rewinding the
	 * stream) */
	sort_and_uniq (tlist);

	/* add the sentinel (token=0) */
	tlist_add (tlist, 0);

	return tlist->tokens;
}

/*****************************************************************************/

uint32_t *ft_search_tokenize (const char *string)
{
	struct token_list tlist;

	if (string == NULL)
		return NULL;

	tlist_init (&tlist);
	tlist_addstr (&tlist, string);

	return tlist_finish (&tlist);
}


#if 0
uint32_t *ft_search_tokenizef (Share *file)
{
	struct token_list tlist;

	if (file == NULL)
		return NULL;

	tlist_init (&tlist);

	tlist_addstr (&tlist, SHARE_DATA(file)->path);
	tlist_addstr (&tlist, share_get_meta (file, "tracknumber"));
	tlist_addstr (&tlist, share_get_meta (file, "artist"));
	tlist_addstr (&tlist, share_get_meta (file, "album"));
	tlist_addstr (&tlist, share_get_meta (file, "title"));
	tlist_addstr (&tlist, share_get_meta (file, "genre"));

	return tlist_finish (&tlist);
}

#endif

#define TEST "/mnt/max/2/dvdrips/dvd divx rip avi xvid ogm/Corrs - Live At The Royal Albert Hall/Corrs - Live At The Royal Albert Hall - 12 - Runaway.ogg"
int main(int argc, char *argv[])
{
	int i;
	int num;
	int32_t *tl, *ptr;

	num = atoi (argv[1]);

	for (i=0; i<num; i++)
	{
		tl = ft_search_tokenize (TEST);

		free (tl);
	}

	tl = ft_search_tokenize (TEST);
	for (ptr = tl; *ptr; ptr++)
		printf ("%x\t", *ptr);

	putchar('\n');

	return 0;
}
