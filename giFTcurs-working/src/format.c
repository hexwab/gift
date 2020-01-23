/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001-2002 Göran Weinholt <weinholt@linux.nu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: format.c,v 1.27 2002/12/01 23:29:24 chnix Exp $
 */
#include "giftcurs.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

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

static unsigned int disk_free_kb(void)
{
	/* Cache values so we don't need to call statfs so often */
	static unsigned int cached_free_kb;
	static tick_t cache_expires;

	tick_t now = uptime();

	if (now > cache_expires) {
		struct statfs statbuf;

		cache_expires = now + SECS(5);

		if (statfs(download_dir, &statbuf) < 0) {
			ERROR("statfs");
			cached_free_kb = -1;
			return -1;
		}
		cached_free_kb = (statbuf.f_bsize / 256) * statbuf.f_bavail / 4;
	}
	return cached_free_kb;
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
			char *flags;
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
			char *flags;
		} fixed;
	} u;
	int refcount;
};

typedef char *(*getattrF) (void *udata, char *key, unsigned int *intval);

static char *format_expand(format_t format, getattrF, int maxlen, void *udata);

/* This describes how to get meta data from a hit */
static char *hit_getattr(void *udata, char *key, unsigned int *intval)
{
	hit *h = udata;

	if (!strcmp(key, "hash"))
		return h->hash;
	if (!strcmp(key, "filename"))
		return h->filename;
	if (!strcmp(key, "path"))
		return h->directory;
	if (!strcmp(key, "filesize")) {
		*intval = h->filesize;
		return NULL;
	}
	if (!strcmp(key, "sources")) {
		*intval = h->sources.num;
		return NULL;
	}
	if (!strcmp(key, "availability")) {
		int i, avail = 0;

		for (i = 0; i < h->sources.num; i++) {
			subhit *sh = list_index(&h->sources, i);

			avail += !!sh->availability;
		}
		*intval = avail;
		return NULL;
	}
	if (!strcmp(key, "downloading")) {
		*intval = h->downloading;
		return NULL;
	}
	if (!strcmp(key, "expanded")) {
		*intval = h->expanded;
		return NULL;
	}

	return meta_data_lookup(h, key);
}

/* This describes how to get meta data from a subhit */
static char *subhit_getattr(void *udata, char *key, unsigned int *intval)
{
	subhit *h = udata;

	if (!strcmp(key, "user"))
		return h->user;
	if (!strcmp(key, "url"))
		return h->href;
	if (!strcmp(key, "availability")) {
		*intval = h->availability;
		return NULL;
	}
	if (!strcmp(key, "net")) {
		static char net[16];
		char *colon = strchr(h->href, ':');
		int i;

		if (!colon)
			return NULL;
		i = colon - h->href;

		if (i >= sizeof net)
			i = sizeof net - 1;
		memcpy(net, h->href, i);
		net[i] = '\0';
		return net;
	}
	return hit_getattr(h->parent, key, intval);
}

/* This describes how to get meta data from a transfer */
static char *transfer_getattr(void *udata, char *key, unsigned int *intval)
{
	transfer *t = udata;

	if (!strcmp(key, "filename"))
		return t->filename;
	if (!strcmp(key, "filesize")) {
		*intval = t->filesize;
		return NULL;
	}
	if (!strcmp(key, "bandwidth")) {
		*intval = t->bandwidth;
		return NULL;
	}
	if (!strcmp(key, "ratio")) {
		if (t->filesize > UINT_MAX / 200)
			*intval = 100 * (t->transferred >> 8) / (t->filesize >> 8);
		else if (t->filesize)
			*intval = 100 * t->transferred / t->filesize;
		return NULL;
	}
	if (!strcmp(key, "transferred")) {
		*intval = t->transferred;
		return NULL;
	}
	if (!strcmp(key, "searching")) {
		*intval = !!t->id;
		return NULL;
	}
	if (!strcmp(key, "expanded")) {
		*intval = t->expanded;
		return NULL;
	}
	if (!strcmp(key, "status"))
		return _(t->status);
	if (!strcmp(key, "active")) {
		*intval = !!t->id && !t->paused;
		return NULL;
	}
	if (!strcmp(key, "eta")) {
		if (t->bandwidth)
			*intval = (t->filesize - t->transferred) / t->bandwidth;
		return NULL;
	}
	/* This is not related to this transfer, but may be interesting anyway */
	if (!strcmp(key, "disk_free")) {
#if HAVE_STATFS
		*intval = disk_free_kb();
#else
		*intval = (unsigned int) -1;
#endif
		return NULL;
	}
	return NULL;
}

/* This describes how to get meta data from a source */
static char *source_getattr(void *udata, char *key, unsigned int *intval)
{
	source *h = udata;

	if (!strcmp(key, "user"))
		return h->user;
	if (!strcmp(key, "url"))
		return h->href;
	if (!strcmp(key, "start")) {
		*intval = h->start;
		return NULL;
	}
	if (!strcmp(key, "transferred")) {
		*intval = h->transmit;
		return NULL;
	}
	if (!strcmp(key, "total")) {
		*intval = h->total;
		return NULL;
	}
	if (!strcmp(key, "net")) {
		static char net[16];
		int i = strchr(h->href, ':') - h->href;

		if (i >= sizeof net)
			i = sizeof net - 1;
		memcpy(net, h->href, i);
		net[i] = '\0';
		return net;
	}
	if (!strcmp(key, "status")) {
		if (!h->status)
			return _("Unknown status");

		/* XXX This is ugly. Isn't there any better way to do i18n? */
		/* Queued (\d+) */
		/* Queued (position \d+) */
		if (!strncmp(h->status, "Queued (", 8) && isdigit(h->status[8])) {
			static char foo[64];

			snprintf(foo, sizeof foo, _("Queued (%d)"), atoi(h->status + 8));
			return foo;
		}
		if (!strncmp(h->status, "Queued (position ", 17) && isdigit(h->status[17])) {
			static char foo[64];

			snprintf(foo, sizeof foo, _("Queued (position %d)"), atoi(h->status + 17));
			return foo;
		}

		return _(h->status);
	}
	return transfer_getattr(h->parent, key, intval);
}

char *format_hit(format_t format, hit * h, int width)
{
	return format_expand(format, hit_getattr, width, h);
}
char *format_subhit(format_t format, subhit * h, int width)
{
	return format_expand(format, subhit_getattr, width, h);
}
char *format_transfer(format_t format, transfer * t, int width)
{
	return format_expand(format, transfer_getattr, width, t);
}
char *format_source(format_t format, source * t, int width)
{
	return format_expand(format, source_getattr, width, t);
}

struct macro {
	char *id;
	format_t expansion;
};

static list macros = LIST_INITIALIZER;

/* This loads new macros from configuration file on demand */
/* It also prevents recursion, by returning NULL the second time */
format_t format_load(char *id, char *standard)
{
	int i;
	char *format;
	struct macro *m;

	assert(id);

	for (i = 0; i < macros.num; i++) {
		m = list_index(&macros, i);
		if (!strcmp(m->id, id))
			return m->expansion;
	}

	m = calloc(1, sizeof *m);
	m->id = strdup(id);
	m->expansion = NULL;
	list_append(&macros, m);

	format = get_config("format", id, NULL);
	if (!format) {
		if (!standard)
			return NULL;
		format = standard;
	}

	m->expansion = format_compile(format);
	if (!m->expansion)
		message("Format %s won't compile.", id);
	return m->expansion;
}

format_t format_get(char *id, char *standard)
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
		assert(node->refcount > 0);
		node->refcount++;
	}
	return node;
}

static format_t new_node(enum type_enum type, format_t next)
{
	format_t ret = calloc(1, sizeof *ret);

	ret->type = type;
	ret->next = next;
	ret->refcount = 1;
	return ret;
}

struct format_ctrl {
	getattrF getattr;
	void *udata;
	int space_len;
	int spaces;
	int slack;
	int total_len;
};

static unsigned int attribute_int(char *attr, struct format_ctrl *k);

static char *attribute_get(char *attr, struct format_ctrl *k, unsigned int *intval)
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
		 * Otherwise (x < 5) will return max(x,1) because 0 means false. */
		*intval = a < b ? (a ? a : 1) : 0;
		return NULL;
	}
	if (isdigit(attr[0]))
		return attr;
	return k->getattr(k->udata, attr, intval);
}

static unsigned int attribute_int(char *attr, struct format_ctrl *k)
{
	char *s;
	int n = 0;

	s = attribute_get(attr, k, &n);
	if (n)
		return n;
	return my_atoi(s);
}

struct format_counters {
	int spaces;
	int n;
	int nonprint;
	int variable;
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

	for (n = 0; i; n++)
		i /= 10;
	return n;
}
static int max_int(int a, int b)
{
	return a < b ? b : a;
}
static int percent_atoi(char *s, int w)
{
	char *endp;
	int value = strtoul(s, &endp, 10);

	if (*endp == '%')
		value = value * w / 100;
	return value;
}

/* strdup an identifier and arguments from str. backwards. */
static int read_id(char *str, int n, char **command, char **args)
{
	int start;

	assert(str[n] == '}');
	n--;

	for (start = n; str[start] != '{'; start--)
		if (start < 0) {
			DEBUG("Unexpected `}'");
			*command = NULL;
			*args = NULL;
			return n;
		}
	*command = str = strndup(str + start + 1, n - start);

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

format_t format_compile(char *src)
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
			pos->u.text = strdup(bufp);
			bufp = buf + sizeof buf - 1;
		}

		/* we have a command ending with } */
		n = read_id(src, n, &command, &args);
		if (!command) {
			format_unref(pos);
			format_unref(fixedpos);
			return NULL;
		}
		strlower(command);
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
			pos->u.cond.cond = my_strdup(args);
			pos->u.cond.jump = pending;
			format_unref(endif);
			pending = endif = NULL;
		} else if (!strcmp(command, "else")) {
			tmp = pending;
			pending = pos;
			pos = tmp;
		} else if (!strcmp(command, "elif")) {
			tmp = new_node(COND, pos);
			tmp->u.cond.cond = my_strdup(args);
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
			pos->u.progress.total = strdup(sep);
			sep = strtok(NULL, " ");
			if (!sep || !*sep) {
				DEBUG("{progress} expects at least two arguments");
				format_unref(pos);
				format_unref(fixedpos);
				return NULL;
			}
			pos->u.progress.end = strdup(sep);
			sep = strtok(NULL, " ");
			if (sep && *sep) {
				/* We have three arguments: {progress total start end} */
				pos->u.progress.start = pos->u.progress.end;
				pos->u.progress.end = strdup(sep);
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
			pos->u.fixed.flags = strdup(args);
			pos->u.fixed.body = tmp;
			endif = o_endif;
			pending = o_pending;
			progress_on = o_progress_on;

			fixedpos = NULL;
			fixed_on = 0;
		} else {
			/* we have an attribute (meta data key) */
			pos = new_node(ATTR, pos);
			pos->u.attr.name = strdup(command);
			pos->u.attr.flags = my_strdup(args);
		}
		free(command);
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
		pos->u.text = strdup(bufp);
		bufp = buf + sizeof buf - 1;
	}
	return pos;
}

void format_unref(format_t n)
{
	if (!n)
		return;
	assert(n->refcount > 0);
	if (--n->refcount) {
		return;
	}
	switch (n->type) {
	case COND:
		free(n->u.cond.cond);
		format_unref(n->u.cond.jump);
		break;
	case ATTR:
		free(n->u.attr.name);
		free(n->u.attr.flags);
		break;
	case TEXT:
		free(n->u.text);
		break;
	case PROGRESS:
		free(n->u.progress.total);
		free(n->u.progress.start);
		free(n->u.progress.end);
		break;
	case ENDPROGRESS:
	case COLOR:
	case SPACE:
		break;
	case MAKRO:
		format_unref(n->u.macro);
		break;
	case FIXED:
		free(n->u.fixed.flags);
		format_unref(n->u.fixed.body);
		break;
	}
	format_unref(n->next);
	free(n);
}

/* Discover how long this format will be when it's printed, in terms of
 * total characters, nonprinted (color) characters, and {space}s */
static struct format_counters format_collect(format_t n, struct format_ctrl *k)
{
	struct format_counters c = { 0, 0, 0, 0 };

	while (n) {
		switch (n->type) {
			int width;
			unsigned int i;
			char *s, *flags;

		case COND:
			if (!attribute_int(n->u.cond.cond, k)) {
				n = n->u.cond.jump;
				continue;
			}
			break;
		case ATTR:
			i = -1;
			s = attribute_get(n->u.attr.name, k, &i);
			flags = n->u.attr.flags;
			if (flags == NULL || flags[0] == '\0') {
				if (s) {
					c.n += strlen(s);
					if (!flags)
						c.variable += strlen(s);
				} else if (i != -1) {
					c.n += digits(i);
				}
			} else if (isdigit(flags[0])) {
				width = percent_atoi(flags, k->total_len);
				if (i != -1)
					c.n += max_int(width, digits(i));
				else
					c.n += width;
			} else if (flags[0] == '-') {
				flags++;
				width = percent_atoi(flags, k->total_len);
				if (i != -1)
					c.n += max_int(width, digits(i));
				else
					c.n += width;
			} else if (flags[0] == 't') {
				c.n += 6;
			} else if (strchr(prefixchars, flags[0])) {
				c.n += 4;
			} else {
				DEBUG("Unknown format '%s' -- ignored", flags);
			}
			break;
		case TEXT:
			c.n += strlen(n->u.text);
			break;
		case PROGRESS:
			c.nonprint += 4;
			c.n += 4;
			break;
		case ENDPROGRESS:
			break;
		case COLOR:
			c.nonprint += 2;
			c.n += 2;
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
				int len = percent_atoi(n->u.fixed.flags, old_maxlen);
				int more;

				k->total_len = len;
				more = format_collect(n->u.fixed.body, k).nonprint;
				k->total_len = old_maxlen;
				c.nonprint += more;
				c.n += len + more;
			}
			break;
		}
		n = n->next;
	}
	return c;
}

/* Fill str with the actual data. All rendering info is provided in k */
static void format_produce(format_t n, struct format_ctrl *k, char *str)
{
	char *progress_mark = NULL;
	float progress_start = 0.0;
	float progress_end = 0.0;

	while (n) {
		str += strlen(str);
		switch (n->type) {
			int width;
			unsigned int i;
			char *s, *p, *flags;

		case COND:
			if (!attribute_int(n->u.cond.cond, k)) {
				n = n->u.cond.jump;
				continue;
			}
			break;
		case ATTR:
			i = -1;
			s = attribute_get(n->u.attr.name, k, &i);
			flags = n->u.attr.flags;
			if (flags == NULL || flags[0] == '\0') {
				if (s && n->u.attr.flags == NULL) {
					int l = strlen(s) + k->slack;

					if (l <= 0) {
						k->slack = l;
					} else {
						sprintf(str, "%*.*s", l, l, s);
						k->slack = 0;
					}
				} else if (s) {
					strcpy(str, s);
				} else if (i != -1) {
					sprintf(str, "%u", i);
				}
			} else if (isdigit(flags[0])) {
				width = percent_atoi(flags, k->total_len);
				if (s)
					sprintf(str, "%*.*s", width, width, s);
				else if (i != -1)
					sprintf(str, "%*u", width, i);
				else
					spacefill(str, width);
			} else if (flags[0] == '-') {
				width = percent_atoi(flags + 1, k->total_len);
				if (s)
					sprintf(str, "%-*.*s", width, width, s);
				else if (i != -1)
					sprintf(str, "%-*u", width, i);
				else
					spacefill(str, width);
			} else if (flags[0] == 't') {
				if (i == -1 && s == NULL)
					spacefill(str, 6);
				else {
					if (i == -1)
						i = atoi(s);
					strcpy(str, humanify_time(i));
				}
			} else if ((p = strchr(prefixchars, flags[0]))) {
				if (i == -1 && s == NULL)
					spacefill(str, 4);
				else {
					int base = flags[1] == 'i' ? 1024 : 1000;

					if (i == -1)
						i = atoi(s);
					strcpy(str, humanify_scale_base(i, p - prefixchars, base));
				}
			} else {
				DEBUG("Unknown format '%s' -- ignored", n->u.attr.flags);
			}
			break;
		case TEXT:
			strcpy(str, n->u.text);
			break;
		case PROGRESS:
			i = attribute_int(n->u.progress.total, k);
			if (i) {
				unsigned int j;

				if (n->u.progress.start) {
					j = attribute_int(n->u.progress.start, k);

					if (j >= i)
						progress_start = 1.0;
					else
						progress_start = (float) j / (float) i;
				} else {
					progress_start = 0.0;
					j = 0;
				}

				j += attribute_int(n->u.progress.end, k);

				if (j >= i)
					progress_end = 1.0;
				else
					progress_end = (float) j / (float) i;

				progress_mark = str;
			} else {
				progress_mark = NULL;
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
			spacefill(str, k->space_len / k->spaces);
			k->space_len -= k->space_len / k->spaces;
			k->spaces--;
			break;
		case MAKRO:
			format_produce(n->u.macro, k, str);
			break;
		case FIXED:
			{
				int length = percent_atoi(n->u.fixed.flags, k->total_len);
				char *substr;

				substr = format_expand(n->u.fixed.body, k->getattr, length, k->udata);
				strcpy(str, substr);
				free(substr);
			}
			break;
		}
		n = n->next;
	}
}

/* Returns a newly allocaded string with udata formatted according to format */
char *format_expand(format_t format, getattrF getattr, int maxlen, void *udata)
{
	/* size is array size in bytes, lenght is the printed out lenght */
	struct format_ctrl morot;
	struct format_counters c;
	char *str;
	int length;
	int slack;
	int alloc_size;
	int underrun;

	morot.getattr = getattr;
	morot.udata = udata;
	morot.total_len = maxlen;
	c = format_collect(format, &morot);
	length = c.n - c.nonprint;

	/* Split the space equally among spaces */
	slack = maxlen - length;
	morot.space_len = 0;
	morot.spaces = c.spaces;
	if (slack > 0 && c.spaces) {
		morot.space_len = slack;
		slack = 0;
	}
	underrun = -slack - c.variable;
	if (underrun > 0) {
		/* Oops. The variable-length formats can't shrink enough... */
		/* XXX: Do some heroic stuff here instead */
		DEBUG("Formatted string too long.. increasing maxlen from %d to %d",
			  maxlen, maxlen + underrun);
		maxlen += underrun;
		slack = -c.variable;
	}
	morot.slack = slack;
	alloc_size = maxlen + c.nonprint;
	str = malloc(alloc_size + 1);
	str[0] = '\0';

	/* Fill the string with actual stuff */
	format_produce(format, &morot, str);

	/* if the string became too short, pad with spaces */
	if (strlen(str) < alloc_size)
		spacefill(str + strlen(str), alloc_size - strlen(str));

	//DEBUG("planned size=%d, actual=%d", alloc_size, strlen(str));
	return str;
}

void format_clear(void)
{
	int i;

	for (i = 0; i < macros.num; i++) {
		struct macro *m = list_index(&macros, i);

		free(m->id);
		format_unref(m->expansion);
	}
	list_free_entries(&macros);
}
