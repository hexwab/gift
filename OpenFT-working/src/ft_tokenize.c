/*
 * $Id$
 *
 * Copyright (C) 2001-2004 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "ft_openft.h"
#include <ctype.h>

#include "ft_tokenize.h"

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
#define SEARCH_TOKEN_DELIM "\\\"/ _-.[]()\t"

/*****************************************************************************/

static BOOL is_token_punct (int c)
{
	/* TODO: Lots of room for optimization here */
	return BOOL_EXPR(strchr (SEARCH_TOKEN_PUNCT, c));
}

static int next_letter (const unsigned char **strref, size_t *lenref)
{
	const unsigned char *str = *strref;
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
static uint32_t make_token (const unsigned char *word, size_t len)
{
	uint32_t hash = 0;
	int letter;

	if (word == NULL)
		return 0;

	if ((letter = next_letter (&word, &len)) == 0)
		return 0;

	hash = letter;

	while ((letter = next_letter (&word, &len)) != 0)
	  //		hash = (hash << 5) - hash + letter;
	  hash = (hash * 16777619) ^ letter;

	/* just in case */
	if (hash == 0)
		hash++;

	return hash;// & ((1<<18) -1);
}

/*****************************************************************************/

struct token_list
{
	/* List of unique tokens */ 
	uint32_t *tokens;
	size_t    nmemb;
	size_t    size;

	/* Order of tokens */
	uint8_t  *order;
	size_t    ordlen;
	size_t    ordsize;
	
	/* 
	 * Temporary value to avoid backtracking when handling single
	 * tokens. This avoids a malloc in the case of the input
	 * consisting only of unordered single tokens.
	 */
	uint8_t   ordtmp;
	
	/* whether the next token to be written should be preceded by
	 * a separator */
	BOOL      separate;

	/* whether the separator character should toggle quotedness
	 * instead of inserting a separator */
	BOOL      quote_mode;
	BOOL      quoted;
};

static void tlist_init (struct token_list *tlist, BOOL quote_mode)
{
	tlist->tokens = NULL;
	tlist->nmemb = 0;
	tlist->size = 0;
	tlist->order = NULL;
	tlist->ordlen = 0;
	tlist->ordsize = 0;
	tlist->ordtmp = 0;
	tlist->separate = FALSE;
	tlist->quote_mode = quote_mode;
	tlist->quoted = FALSE;
}

static BOOL tlist_resize_min (struct token_list *tlist, size_t nmemb)
{
	uint32_t *newalloc;
	size_t size;

	if (tlist->size >= nmemb)
		return TRUE;

	if ((size = tlist->size) == 0)
		size = 3;

	while (size < nmemb)
		size *= 2;

	if (!(newalloc = realloc (tlist->tokens, size * sizeof (uint32_t))))
		return FALSE;

	tlist->tokens = newalloc;
	tlist->size = size;

	return TRUE;
}

static BOOL order_resize_add (struct token_list *tlist, uint8_t num)
{
	if (tlist->ordlen+1 > tlist->ordsize)
	{
		uint8_t *newalloc;
		size_t size = tlist->ordsize * 2 + 3;

		if (!(newalloc = realloc (tlist->order, size)))
			return FALSE;

		tlist->order = newalloc;
		tlist->ordsize = size;
	}

	tlist->order[tlist->ordlen++] = num;

	return TRUE;
}

static BOOL order_add (struct token_list *tlist, uint8_t num)
{
	if (num <= ORDER_SEP)
	{
		/* less than 2 tokens in current phrase: abort it */
		if (tlist->ordtmp != 1)
		{
			tlist->ordtmp = 0;

			/* make sure the previous sentinel, if any,
			 * was the one we want */
			if (tlist->ordlen)
			{
				assert (tlist->order[tlist->ordlen-1] <= ORDER_SEP);
				tlist->order[tlist->ordlen-1] = num;
			}

			return FALSE;
		}

		/* we've already written the phrase out, so write the
		 * sentinel */
		order_resize_add (tlist, num);
		tlist->ordtmp = 0;
	}
	else 
	{
		/* first token of a phrase: buffer it */
		if (tlist->ordtmp == 0)
		{
			tlist->ordtmp = num;
			return FALSE;
		}

		/* second token: write the buffered one out first */
		if (tlist->ordtmp > 1)
		{
			order_resize_add (tlist, tlist->ordtmp);
			tlist->ordtmp = 1;
		}

		order_resize_add (tlist, num);
	}
			
	return TRUE;
}

static BOOL tlist_add (struct token_list *tlist, uint32_t token, BOOL skip)
{
	int num;
	
	/* check if we've already seen this token */
	for (num = 0; num < tlist->nmemb; num++)
	{
		if (tlist->tokens[num] == token)
			break;
	}
	
	/* if not, add it */
	if (num == tlist->nmemb)
	{
		if (tlist_resize_min (tlist, tlist->nmemb + 1) == FALSE)
			return FALSE;
		
		tlist->tokens[tlist->nmemb++] = token;
	}
	
	/* add to the order list, unless we're skipping this token */
	if (skip == FALSE)
	{
		/* add a separator if we're due for one (this is done
		 * lazily to avoid consecutive separators) */
		if (tlist->separate == TRUE)
		{
			tlist->separate = FALSE;

			order_add (tlist, ORDER_SEP);
		}
		
		/* FIXME: if there are more than 254 unique tokens,
		 * some phrases will unavoidably need discarding; the
		 * current code only discards tokens, not phrases.
		 */
		if (num <= ORDER_MAX-ORDER_MIN)
			order_add (tlist, num + ORDER_MIN);
	}

	return TRUE;
}

static BOOL tlist_addword (struct token_list *tlist,
                           const char *word, size_t wordlen, BOOL skip)
{
	uint32_t token;

	if ((token = make_token (word, wordlen)) > 0)
		return tlist_add (tlist, token, skip);

	return FALSE;
}

static void add_numbers (struct token_list *tlist, const unsigned char *str)
{
	unsigned char *ptr;
	size_t numlen;

	/* implicitly scan past leading 0's as they are found only for padding,
	 * not to represent octal numbers */
	while ((ptr = strpbrk (str, "123456789")))
	{
		numlen = strspn (ptr, "0123456789");
		assert (numlen > 0);

		tlist_addword (tlist, ptr, numlen, TRUE);

		str = ptr + numlen;
	}
}

static void add_words (struct token_list *tlist, const unsigned char *str, int sepchar)
{
	size_t wordlen;

	while (1)
	{
		if ((wordlen = strcspn (str, SEARCH_TOKEN_DELIM)) > 0)
			tlist_addword (tlist, str, wordlen, 
				       tlist->quote_mode && !tlist->quoted);

		/* force a separator at sepchar (which should be
		 * in SEARCH_TOKEN_DELIM), or toggle quotedness */
		if (sepchar && str[wordlen] == sepchar)
		{
			tlist->separate = TRUE;
			
			if (tlist->quote_mode)
				tlist->quoted ^= TRUE;
		}

		if (str[wordlen] == '\0')
			break;

		str += wordlen + 1;
	}
}

static void tlist_addstr (struct token_list *tlist, const unsigned char *str, int sepchar)
{
	/* lets us be lazy in the usage */
	if (str == NULL)
		return;

	if (tlist->quote_mode == FALSE)
		add_numbers (tlist, str);

	add_words (tlist, str, sepchar);

	tlist->separate = TRUE;
}

static uint32_t *tlist_finish (struct token_list *tlist, uint8_t **order)
{
	order_add (tlist, ORDER_END); /* terminator */

	if (order)
		*order = tlist->order;
	else
		free (tlist->order);
	
	/* add the sentinel (token=0) */
	tlist_add (tlist, 0, TRUE);

	return tlist->tokens;
}

/*****************************************************************************/

uint32_t *ft_tokenize_query (const unsigned char *string, uint8_t **order)
{
	struct token_list tlist;

	if (string == NULL)
		return NULL;

	tlist_init (&tlist, TRUE);
	tlist_addstr (&tlist, string, '"');

	return tlist_finish (&tlist, order);
}

uint32_t *ft_tokenize_share (Share *file, uint8_t **order)
{
	struct token_list tlist;

	if (file == NULL)
		return NULL;

	tlist_init (&tlist, FALSE);

	tlist_addstr (&tlist, SHARE_DATA(file)->path, '/');
	tlist_addstr (&tlist, share_get_meta (file, "tracknumber"), '\0');
	tlist_addstr (&tlist, share_get_meta (file, "artist"), '\0');
	tlist_addstr (&tlist, share_get_meta (file, "album"), '\0');
	tlist_addstr (&tlist, share_get_meta (file, "title"), '\0');
	tlist_addstr (&tlist, share_get_meta (file, "genre"), '\0');

	return tlist_finish (&tlist, order);
}

/*****************************************************************************/

#if 0
#define TEST "/mnt/media/share/ogg albumen/Tori Amos/Under the Pink/Tori Amos - Under the Pink - 12 - Yes, Anastasia.ogg"
int main(int argc, char *argv[])
{
	int32_t *tl, *ptr;
	uint8_t *order, *optr;
#if 0
	int i;
	int num;

	num = atoi (argv[1]);

	for (i=0; i<num; i++)
	{
		tl = ft_tokenize_query (TEST, &order);

		free (tl);
		free (order);
	}
#else

	tl = ft_tokenize_query (argv[1], &order);
	for (ptr = tl; *ptr; ptr++)
		printf ("%x\t", *ptr);

	free (tl);

	putchar('\n');

	if (order)
		for (optr = order; *optr; optr++)
			printf ("%d ", *optr);

	putchar('\n');
#endif
	return 0;
}
#endif
