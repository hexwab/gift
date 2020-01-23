/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * giFTcurs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with giFTcurs; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: format.c,v 1.64 2003/06/27 11:20:13 weinholt Exp $
 */
#include "giftcurs.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#include "parse.h"
#include "format.h"
#include "screen.h"
#include "settings.h"

#if HAVE_STATFS
# include <sys/types.h>
# include <sys/param.h>
# if HAVE_SYS_MOUNT_H
#  include <sys/mount.h>
# endif
# if HAVE_SYS_VFS_H
#  include <sys/vfs.h>
# endif
# if HAVE_SYS_STATVFS_H
#  include <sys/statvfs.h>
#  define statfs statvfs
# endif

guint64 disk_free(void)
{
	static char *download_dir = (char *) 1, *download_dir2 = NULL;

	/* Cache values so we don't need to call statfs so often */
	static GTimer *timer = NULL;	/* XXX: this one leaks */
	static guint64 cached_free = -1;

	if (download_dir == (char *) 1) {
		/* Initialize. Get the path to downloaded and completed files */
		/* If server_host is localhost, read download_dir from gift.conf */
		/* This seems like an ugly hack, and it really is :) */
		/* FIXME: This assumes daemon is run by the same user */
		download_dir = NULL;
		if (!strcmp(server_host, "localhost") || !strcmp(server_host, "127.0.0.1")
			|| server_host[0] == '/') {
			char *opt = read_gift_config("../gift.conf", "[download]", "completed");

			if (opt) {
				download_dir = opt;
				DEBUG("got download dir from gift.conf: %s", download_dir);
			}

			opt = read_gift_config("../gift.conf", "[download]", "incoming");

			if (opt) {
				download_dir2 = opt;
				DEBUG("got download dir from gift.conf: %s", download_dir2);
			}
		}
	}

	if (!download_dir)
		return -1;

	if (!timer)
		timer = g_timer_new();

	if (g_timer_elapsed(timer, NULL) > 5.0 || cached_free == -1) {
		struct statfs statbuf;

		g_timer_start(timer);

		if (statfs(download_dir, &statbuf) < 0) {
			DEBUG("statfs(%s): %s (%d)", download_dir, g_strerror(errno), errno);
			g_free(download_dir);
			g_free(download_dir2);
			download_dir = download_dir2 = NULL;
			return -1;
		}

		cached_free = (guint64) statbuf.f_bavail * statbuf.f_bsize;

		/* take the minimum of the two dirs, if they are on different fs */
		if (download_dir2) {
			if (statfs(download_dir2, &statbuf) < 0) {
				DEBUG("statfs(%s): %s (%d)", download_dir2, g_strerror(errno), errno);
				g_free(download_dir2);
				download_dir2 = NULL;
			} else {
				guint64 b = (guint64) statbuf.f_bavail * statbuf.f_bsize;

				if (b < cached_free)
					cached_free = b;
			}
		}
	}
	return cached_free;
}
#else
guint64 disk_free_kb(void)
{
	return -1;
}

#endif

/* see manual page giFTcurs.conf(5) for details */

struct precompiled {
	enum type_enum { COND, ATTR, TEXT, PROGRESS, ENDPROGRESS, COLOR, SPACE, MAKRO, FIXED } type;
	format_t next;
	union {
		struct {
			char *cond;
			format_t jump;
		} cond;
		struct {
			char *name;
			int width;
			char mode;
			char scale;
			unsigned left_justify:1;
		} attr;
		char *text;
		struct {
			int color;
			int bold;
		} color;
		format_t macro;
		struct {
			char *start;
			char *end;
			char *total;
		} progress;
		struct {
			format_t body;
			int width;
			unsigned percent:1;
		} fixed;
	} u;
	int refcount;
};

/* Funcion that allocates and fills in a sort string.
 * The format string should be like "attrib1,-attrib2,+attrib3"
 * and attributes are looked up in the passed function.
 * Result is stored at the res pointer. Note that this is not a
 * nul-terminated string, rather a binary area of numbers/strings mixed.
 * Return 1 if sortkey changed, 0 if not. */

int make_sortkey(char **res, const void *item, const char *format, getattrF getattr)
{
	int rev, len;
	char *comma, *fmt, *fmt0;
	char buf[1000], *bufp = buf;
	attr_value v;

	fmt0 = fmt = g_strdup(format);

	do {
		comma = strchr(fmt, ',');
		if (comma)
			*comma = '\0';
		rev = fmt[0] == '-';
		if (!isalpha(fmt[0]))
			fmt++;

		len = 80;
		switch (getattr(item, fmt, &v)) {
		case ATTR_STRLEN:
			len = v.strlen.len;
		case ATTR_STRING:
			len = strxfrm(bufp, v.string, len) + 1;
			if (rev) {
				int i;

				for (i = 0; i < len; i++)
					bufp[i] = ~bufp[i];
			}
			bufp += len;
			break;
		case ATTR_INT:
			/* big endian enables us to use memcmp */
			if (!rev)
				v.intval = ~v.intval;
			*((unsigned int *) bufp)++ = GINT_TO_BE(v.intval);
			break;
		case ATTR_LONG:
			/* big endian enables us to use memcmp */
			if (!rev)
				v.longval = ~v.longval;
			*((guint64 *) bufp)++ = GINT64_TO_BE(v.intval);
			break;
		case ATTR_NONE:
			break;
		}
		if (!comma)
			break;
		fmt = comma + 1;
	} while (bufp < buf + sizeof buf - 160);

	g_free(fmt0);

	/* end with the pointer value to prevent equal strings */
	*((const void **) bufp)++ = item;

	if (*res && memcmp(*res, buf, bufp - buf) == 0)
		return 0;
	g_free(*res);
	*res = g_new(char, bufp - buf);
	memcpy(*res, buf, bufp - buf);
	return 1;
}

struct macro {
	char *id;
	format_t expansion;
};

static dynarray macros = { 0 };

/* This loads new macros from configuration file on demand */
/* It also prevents recursion, by returning NULL the second time */
format_t format_load(const char *id, const char *standard)
{
	int i;
	const char *format;
	struct macro *m;

	g_assert(id);

	for (i = 0; i < macros.num; i++) {
		m = list_index(&macros, i);
		if (!strcmp(m->id, id))
			return m->expansion;
	}

	m = g_new(struct macro, 1);
	m->id = g_strdup(id);
	m->expansion = NULL;
	dynarray_append(&macros, m);

	format = get_config("format", id, NULL);
	if (!format) {
		if (!standard)
			return NULL;
		format = standard;
	}

	m->expansion = format_compile(format);
	if (!m->expansion)
		g_message("Format %s won't compile.", id);
	return m->expansion;
}

format_t format_get(const char *id, const char *standard)
{
	return format_ref(format_load(id, standard));
}

/*******************************************************/
/* No user readable code below.   Do not read anything */
/* after this line unless you know what you are doing. */
/*******************************************************/

format_t format_ref(format_t node)
{
	if (node) {
		g_assert(node->refcount > 0);
		node->refcount++;
	}
	return node;
}

static format_t new_node(enum type_enum type, format_t next)
{
	format_t ret = g_new0(struct precompiled, 1);

	ret->type = type;
	ret->next = next;
	ret->refcount = 1;
	return ret;
}

struct format_ctrl {
	getattrF getattr;
	const void *udata;
	int space_len;
	int spaces;
	int slack;
	int total_len;
};

static void spacefill(char *s, int spaces)
{
	while (spaces--)
		*s++ = ' ';
	*s = '\0';
}

static int digits(unsigned int i)
{
	int n;

	if (i == 0)
		return 1;
	for (n = 0; i; n++)
		i /= 10;
	return n;
}

static int value_to_int(enum attr_type t, attr_value * v)
{
	switch (t) {
	case ATTR_STRLEN:
		return atoi(v->strlen.string);
	case ATTR_STRING:
		return atoi(v->string);
	case ATTR_INT:
		return v->intval;
	case ATTR_LONG:
		return (int) v->longval;
	case ATTR_NONE:
		return 0;
	}
	abort();
}

static guint64 value_to_long(enum attr_type t, attr_value * v)
{
	if (t == ATTR_LONG)
		return v->longval;
	else
		return value_to_int(t, v);
}

static int value_to_width(enum attr_type t, attr_value * v)
{
	switch (t) {
	case ATTR_STRLEN:
		return v->strlen.len;
	case ATTR_STRING:
		return strlen(v->string);
	case ATTR_INT:
		return digits(v->intval);
	case ATTR_LONG:
		return digits((int) v->longval);
	case ATTR_NONE:
		return 0;
	}
	abort();
}

static const char *value_to_string(enum attr_type t, attr_value * v)
{
	switch (t) {
	case ATTR_STRLEN:
		return v->strlen.string;
	case ATTR_STRING:
		return v->string;
	case ATTR_INT:
		return itoa(v->intval);
	case ATTR_LONG:
		return itoa((int) v->longval);
	case ATTR_NONE:
		return "";
	}
	abort();
}

static unsigned int attribute_int(char *attr, struct format_ctrl *k);

static enum attr_type attribute_get(char *attr, struct format_ctrl *k, attr_value * value)
{
	char *s;

	if ((s = strchr(attr, '<'))) {
		/* We have attr<attr */
		unsigned int a, b;

		*s = '\0';
		a = attribute_int(attr, k);
		b = attribute_int(s + 1, k);
		*s = '<';

		/* Note: To allow constructs like 3<x<5, we group it like 3 < (x < 5)
		 * and makes (x < 5) return 0 if false. That way 3 < 0 will always fail
		 * Therefore, if x < y succeeds, it will return max(x,1). */
		value->intval = a < b ? (a ? a : 1) : 0;
		return ATTR_INT;
	}

	if (isdigit(attr[0])) {
		value->string = attr;
		return ATTR_STRING;
	}
	return k->getattr(k->udata, attr, value);
}

static unsigned int attribute_int(char *attr, struct format_ctrl *k)
{
	attr_value value;

	return value_to_int(attribute_get(attr, k, &value), &value);
}

static guint64 attribute_long(char *attr, struct format_ctrl *k)
{
	attr_value value;

	return value_to_long(attribute_get(attr, k, &value), &value);
}

struct format_counters {
	int spaces;
	int n;
	int nonprint;
	int variable;
};

static int percent_atoi(const char *s, int *value)
{
	char *endp;

	*value = strtoul(s, &endp, 10);

	if (*endp == '%')
		return 1;
	return 0;
}

/* strdup an identifier and arguments from str. backwards. */
static int read_id(const char *string, int n, char **command, char **args)
{
	int start;
	char *str;

	g_assert(string[n] == '}');
	n--;

	for (start = n; string[start] != '{'; start--)
		if (start < 0) {
			DEBUG("Unexpected `}'");
			*command = NULL;
			*args = NULL;
			return n;
		}
	*command = str = g_ascii_strdown(string + start + 1, n - start);

	while (isalnum(*str) || (*str && strchr("_-%<$", *str)))
		str++;
	if (*str == '\0') {
		*args = NULL;
	} else {
		*str = '\0';
		*args = str + 1;
	}
	return start - 1;
}

/* This function takes a string, parses it and returns a linked list of
 * atoms. It parses it backwards for practical purposes. If an error occurs,
 * A message is printed, and NULL is returned. */

format_t format_compile(const char *src)
{
	/* parse string backwards */
	format_t pos = NULL, endif = NULL, pending = NULL, fixedpos = NULL;
	int n = strlen(src);
	char buf[256];
	char *bufp = buf + sizeof buf - 1;
	int progress_on = 0;
	int fixed_on = 0;

	/* temporary space for saved variables (when inside {fixed} {endfixed} */
	format_t o_endif = NULL, o_pending = NULL;
	int o_progress_on = 0;

	*bufp = '\0';

	while (n >= 0) {
		char *command, *args;
		format_t tmp;

		if (n > 0 && src[n - 1] == '\\') {
			/* escaped character */
			*--bufp = src[n];
			n -= 2;
			continue;
		}
		if (src[n] != '}') {
			/* ordinary character */
			*--bufp = src[n];
			n--;
			continue;
		}

		if (*bufp) {
			/* ordinary characters to declare */
			pos = new_node(TEXT, pos);
			pos->u.text = g_strdup(bufp);
			bufp = buf + sizeof buf - 1;
		}

		/* we have a command ending with } */
		n = read_id(src, n, &command, &args);
		if (!command) {
			format_unref(pos);
			format_unref(fixedpos);
			return NULL;
		}
		if (command[0] == '%') {
			int color = get_item_number(command + 1);

			if (color < 0) {
				DEBUG("No color named %s", command + 1);
				format_unref(pos);
				format_unref(fixedpos);
				return NULL;
			}
			if (progress_on) {
				DEBUG("Colors not allowed inside {progress}{endprogress}");
			} else {
				pos = new_node(COLOR, pos);
				pos->u.color.color = color;
				pos->u.color.bold = !!args;
			}
		} else if (command[0] == '$') {
			/* we have a macro to expand */
			tmp = format_get(command, NULL);
			if (!tmp) {
				DEBUG("No macro named %s defined", command);
			} else {
				pos = new_node(MAKRO, pos);
				pos->u.macro = tmp;
			}
		} else if (!strcmp(command, "if")) {
			pos = new_node(COND, pos);
			pos->u.cond.cond = g_strdup(args);
			pos->u.cond.jump = pending;
			format_unref(endif);
			pending = endif = NULL;
		} else if (!strcmp(command, "else")) {
			tmp = pending;
			pending = pos;
			pos = tmp;
		} else if (!strcmp(command, "elif")) {
			tmp = new_node(COND, pos);
			tmp->u.cond.cond = g_strdup(args);
			tmp->u.cond.jump = pending;
			pending = tmp;
			pos = format_ref(endif);
		} else if (!strcmp(command, "endif")) {
			if (pending || endif) {
				DEBUG("unexpected {endif}");
				format_unref(pos);
				format_unref(fixedpos);
				return NULL;
			}
			endif = format_ref(pos);
			pending = format_ref(pos);
		} else if (!strcmp(command, "space")) {
			pos = new_node(SPACE, pos);
		} else if (!strcmp(command, "progress")) {
			char *sep;

			if (!progress_on) {
				DEBUG("{progress} without {endprogress}");
				format_unref(pos);
				format_unref(fixedpos);
				return NULL;
			}
			pos = new_node(PROGRESS, pos);
			sep = strtok(args, " ");
			if (!sep || !sep[0]) {
				DEBUG("{progress} expects at least two arguments");
				format_unref(pos);
				format_unref(fixedpos);
				return NULL;
			}
			pos->u.progress.total = g_strdup(sep);
			sep = strtok(NULL, " ");
			if (!sep || !*sep) {
				DEBUG("{progress} expects at least two arguments");
				format_unref(pos);
				format_unref(fixedpos);
				return NULL;
			}
			pos->u.progress.end = g_strdup(sep);
			sep = strtok(NULL, " ");
			if (sep && *sep) {
				/* We have three arguments: {progress total start end} */
				pos->u.progress.start = pos->u.progress.end;
				pos->u.progress.end = g_strdup(sep);
			}
		} else if (!strcmp(command, "endprogress")) {
			if (progress_on) {
				DEBUG("unexpected {endprogress}");
				format_unref(pos);
				format_unref(fixedpos);
				return NULL;
			}
			progress_on = 1;
			pos = new_node(ENDPROGRESS, pos);
		} else if (!strcmp(command, "endfixed")) {
			if (fixed_on) {
				DEBUG("unexpected {endfixed}");
				format_unref(pos);
				format_unref(fixedpos);
				return NULL;
			}
			fixedpos = pos;
			o_endif = endif;
			o_pending = pending;
			o_progress_on = progress_on;

			endif = pending = pos = NULL;
			progress_on = 0;
			fixed_on = 1;
		} else if (!strcmp(command, "fixed")) {
			if (!fixed_on) {
				DEBUG("{fixed} without {endfixed}");
				format_unref(pos);
				return NULL;
			}
			if (!args) {
				DEBUG("{fixed} must have an argument");
				format_unref(pos);
				return NULL;
			}
			tmp = pos;
			pos = new_node(FIXED, fixedpos);
			pos->u.fixed.percent = percent_atoi(args, &pos->u.fixed.width);
			pos->u.fixed.body = tmp;
			endif = o_endif;
			pending = o_pending;
			progress_on = o_progress_on;

			fixedpos = NULL;
			fixed_on = 0;
		} else {
			char *p;

			/* we have an attribute (meta data key) */
			pos = new_node(ATTR, pos);
			pos->u.attr.name = g_strdup(command);

			pos->u.attr.width = 0;

			if (args == NULL) {
				pos->u.attr.mode = 'e';	/* expandable */
			} else if (args[0] == '\0') {
				pos->u.attr.mode = 'f';	/* fixed */
			} else if (isdigit(args[0])) {
				if (percent_atoi(args, &pos->u.attr.width))
					pos->u.attr.mode = '%';
				else
					pos->u.attr.mode = '=';
			} else if (args[0] == '-') {
				args++;
				if (percent_atoi(args, &pos->u.attr.width))
					pos->u.attr.mode = '%';
				else
					pos->u.attr.mode = '=';
				pos->u.attr.left_justify = 1;
			} else if (args[0] == 't') {
				pos->u.attr.width = 6;
				pos->u.attr.mode = 't';
			} else if ((p = strchr(suffixchars, args[0]))) {
				pos->u.attr.width = 4;
				pos->u.attr.mode = args[1] == 'i' ? 'i' : 'b';
				pos->u.attr.scale = p - suffixchars;
			} else {
				DEBUG("Unknown format modifier '%s'", args);
				format_unref(pos);
				return NULL;
			}
		}
		g_free(command);
		command = NULL;
	}
	if (pending) {
		DEBUG("unexpected {endif}");
		format_unref(pos);
		format_unref(fixedpos);
		return NULL;
	}
	if (fixed_on) {
		DEBUG("unexpected {endfixed}");
		format_unref(pos);
		format_unref(fixedpos);
		return NULL;
	}

	if (*bufp) {
		pos = new_node(TEXT, pos);
		pos->u.text = g_strdup(bufp);
		bufp = buf + sizeof buf - 1;
	}
	return pos;
}

void format_unref(format_t n)
{
	if (!n)
		return;
	g_assert(n->refcount > 0);
	if (--n->refcount)
		return;

	switch (n->type) {
	case COND:
		g_free(n->u.cond.cond);
		format_unref(n->u.cond.jump);
		break;
	case ATTR:
		g_free(n->u.attr.name);
		break;
	case TEXT:
		g_free(n->u.text);
		break;
	case PROGRESS:
		g_free(n->u.progress.total);
		g_free(n->u.progress.start);
		g_free(n->u.progress.end);
		break;
	case ENDPROGRESS:
	case COLOR:
	case SPACE:
		break;
	case MAKRO:
		format_unref(n->u.macro);
		break;
	case FIXED:
		format_unref(n->u.fixed.body);
		break;
	}
	format_unref(n->next);
	g_free(n);
}

/* Discover how long this format will be when it's printed, in terms of
 * total characters, nonprinted (color) characters, and {space}s */
static struct format_counters format_collect(format_t n, struct format_ctrl *k)
{
	struct format_counters c = { 0, 0, 0, 0 };

	while (n) {
		switch (n->type) {
			int width;
			char mode;
			attr_value v;
			enum attr_type t;

		case COND:
			if (!attribute_int(n->u.cond.cond, k)) {
				n = n->u.cond.jump;
				continue;
			}
			break;
		case ATTR:
			t = attribute_get(n->u.attr.name, k, &v);
			mode = n->u.attr.mode;
			width = n->u.attr.width;
			if (mode == 'e' || mode == 'f') {	/* expandable or fixed */
				width = value_to_width(t, &v);
				if (mode == 'e')
					c.variable += width;
			} else if (mode == '%') {
				width = width * k->total_len / 100;
			}
			c.n += width;
			break;
		case TEXT:
			c.n += strlen(n->u.text);
			break;
		case PROGRESS:
			c.nonprint += 4;
			break;
		case ENDPROGRESS:
			break;
		case COLOR:
			c.nonprint += 2;
			break;
		case SPACE:
			c.spaces++;
			break;
		case MAKRO:
			{
				struct format_counters c2 = format_collect(n->u.macro, k);

				c.n += c2.n;
				c.nonprint += c2.nonprint;
				c.spaces += c2.spaces;
				c.variable += c2.variable;
			}
			break;
		case FIXED:
			{
				int old_maxlen = k->total_len;
				int len = n->u.fixed.width;

				if (n->u.fixed.percent)
					len = len * old_maxlen / 100;

				k->total_len = len;
				c.nonprint += format_collect(n->u.fixed.body, k).nonprint;
				k->total_len = old_maxlen;
				c.n += len;
			}
			break;
		}
		if (c.n - c.variable > k->total_len) {
			c.nonprint += c.n - c.variable - k->total_len;
			c.n = k->total_len + c.variable;
			break;
		}
		n = n->next;
	}
	return c;
}

/* Fill str with the actual data. All rendering info is provided in k */
static int format_produce(format_t n, struct format_ctrl *k, char *str, int produced)
{
	char *progress_mark = NULL;
	float progress_start = 0.0;
	float progress_end = 0.0;

	while (n) {
		str += strlen(str);
		if (produced > k->total_len) {
			str[k->total_len - produced] = '\0';
			return k->total_len;
		}
		switch (n->type) {
			int i, width, valwidth;
			char mode;
			char *s;
			const char *string;
			attr_value v;
			enum attr_type t;
			guint64 total;

		case COND:
			if (!attribute_int(n->u.cond.cond, k)) {
				n = n->u.cond.jump;
				continue;
			}
			break;
		case ATTR:
			t = attribute_get(n->u.attr.name, k, &v);
			mode = n->u.attr.mode;
			width = n->u.attr.width;
			string = NULL;
			valwidth = -1;

			if (mode == 'e') {	/* expandable */
				width = valwidth = value_to_width(t, &v);

				width += k->slack;

				if (width < 0) {
					k->slack = width;
					width = 0;
				} else {
					k->slack = 0;
				}
			} else if (mode == 'f') {	/* fixed */
				width = valwidth = value_to_width(t, &v);
			} else if (mode == '%' || mode == '=') {
				if (mode == '%')
					width = width * k->total_len / 100;
				valwidth = value_to_width(t, &v);
			} else if (mode == 't') {
				if (t == ATTR_NONE)
					string = "";
				else
					string = humanify_time(value_to_int(t, &v));
			} else {
				g_assert(mode == 'i' || mode == 'b');
				if (t == ATTR_NONE)
					string = "";
				else {
					guint64 val;
					int base = mode == 'i' ? 1024 : 1000;

					val = value_to_long(t, &v);
					for (i = 0; i < n->u.attr.scale; i++)
						val *= base;

					string = humanify_base(val, base, 1000);
				}
			}
			if (string == NULL)
				string = value_to_string(t, &v);
			if (valwidth == -1)
				valwidth = strlen(string);

			/* output the string s with the given width */
			if (valwidth >= width) {
				strncat(str, string, width);
			} else if (n->u.attr.left_justify) {
				strncat(str, string, valwidth);
				strpad(str, width);
			} else {
				strpad(str, width - valwidth);
				strncat(str, string, valwidth);
			}
			str[width] = '\0';
			produced += width;
			break;
		case TEXT:
			strcpy(str, n->u.text);
			produced += strlen(str);
			break;
		case PROGRESS:
			total = attribute_long(n->u.progress.total, k);
			if (total) {
				guint64 start;

				if (n->u.progress.start) {
					start = attribute_long(n->u.progress.start, k);

					if (start >= total)
						progress_start = 1.0;
					else
						progress_start = (float) start / (float) total;
				} else {
					progress_start = 0.0;
					start = 0;
				}

				start += attribute_long(n->u.progress.end, k);

				if (start >= total)
					progress_end = 1.0;
				else
					progress_end = (float) start / (float) total;

				progress_mark = str;
			}
			break;
		case ENDPROGRESS:
			if (progress_mark) {
				i = str - progress_mark;

				s = progress_mark + (int) (progress_end * i);
				memmove(s + 2, s, strlen(s) + 1);
				s[0] = '\v';
				s[1] = 'a' + COLOR_STANDARD - 1;

				s = progress_mark + (int) (progress_start * i);
				memmove(s + 2, s, strlen(s) + 1);
				s[0] = '\v';
				s[1] = '4';
				progress_mark = NULL;
			}
			break;
		case COLOR:
			str[0] = '\v';
			if (n->u.color.bold)
				str[1] = 'A' + n->u.color.color;
			else
				str[1] = 'a' + n->u.color.color;
			str[2] = '\0';
			break;
		case SPACE:
			width = k->space_len / k->spaces;
			spacefill(str, width);
			k->space_len -= width;
			k->spaces--;
			produced += width;
			break;
		case MAKRO:
			produced = format_produce(n->u.macro, k, str, produced);
			break;
		case FIXED:
			{
				int len = n->u.fixed.width;
				char *substr;

				if (n->u.fixed.percent)
					len = len * k->total_len / 100;

				substr = format_expand(n->u.fixed.body, k->getattr, len, k->udata);
				strcpy(str, substr);
				g_free(substr);

				produced += len;
			}
			break;
		}
		n = n->next;
	}
	return produced;
}

/* Returns a newly allocaded string with udata formatted according to format */
char *format_expand(format_t format, getattrF getattr, int maxlen, const void *udata)
{
	/* size is array size in bytes, lenght is the printed out lenght */
	struct format_ctrl morot;
	struct format_counters c;
	char *str;
	int slack;

	morot.getattr = getattr;
	morot.udata = udata;
	morot.total_len = maxlen;
	c = format_collect(format, &morot);

	/* Split the space equally among spaces */
	slack = maxlen - c.n;
	g_assert(slack + c.variable >= 0);

	morot.space_len = 0;
	morot.spaces = c.spaces;
	if (slack > 0 && c.spaces) {
		morot.space_len = slack;
		slack = 0;
	}

	morot.slack = slack;

	str = g_malloc(maxlen + c.nonprint + 1);
	str[0] = '\0';

	/* Fill the string with actual stuff */
	format_produce(format, &morot, str, 0);

	g_assert(maxlen + c.nonprint >= strlen(str));

	return str;
}

void format_clear(void)
{
	int i;

	for (i = 0; i < macros.num; i++) {
		struct macro *m = list_index(&macros, i);

		g_free(m->id);
		format_unref(m->expansion);
		g_free(m);
	}
	dynarray_removeall(&macros);
}
