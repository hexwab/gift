/* giFT OpenNap
 * Copyright (C) 2003 Tilman Sauerbeck <tilman@code-monkey.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#define __OPN_OPENNAP_C
#include "opn_opennap.h"
#include "opn_share.h"
#include "opn_search.h"
#include "opn_download.h"

Protocol *opn_proto = NULL;

BOOL opn_is_connected()
{
	OpnSession *session;
	List *l;
	
	for (l = OPENNAP->sessions; l; l = l->next) {
		session = (OpnSession *) l->data;

		if (session->state == OPN_SESSION_STATE_CONNECTED)
			return TRUE;
	}

	return FALSE;
}

static BOOL opn_connect(timer_id *timer)
{
	OpnSession *session;
	OpnNode *node;
	List *l;

	if (list_length(OPENNAP->sessions) >=
	    config_get_int(OPENNAP->cfg, "main/max_connections=15"))
		return TRUE;
	
	for (l = OPENNAP->nodelist->nodes; l; l = l->next) {
		node = (OpnNode *) l->data;
		
		if (node->state == OPN_NODE_STATE_OFFLINE) {
			session = opn_session_new();
			
			if (!opn_session_connect(session, node))
				opn_session_free(session);
		}
	}

	return TRUE;
}

void main_timer()
{
	timer_id timer = timer_add(30 * SECONDS, (TimerCallback) opn_connect, &timer);
}

static int gift_cb_stats(Protocol *p, unsigned long *users, unsigned long *files, 
                         double *size, Dataset **extra)
{
	OpnSession *session;
	List *l;

	*users = *files = *size = 0;

	for (l = OPENNAP->sessions; l; l = l->next) {
		session = (OpnSession *) l->data;

		*users += session->stats.users;
		*files += session->stats.files;
		*size += session->stats.size;
	}

	return 1;
}

static Config *config_load()
{
	Config *cfg;
	char *src, dst[PATH_MAX + 1];
	
	src = gift_conf_path("OpenNap/OpenNap.conf");

	if (!(cfg = gift_config_new("OpenNap"))) {
		snprintf(dst, sizeof(dst), DATADIR "/OpenNap/OpenNap.conf");
		file_cp(src, dst);

		cfg = gift_config_new("OpenNap");
	}

	return cfg;
}

/* Creates a random username and saves it to OPENNAP->cfg
 */
static void set_username()
{
	char buf[16];
	int i, x;

	srand(time(NULL));

	for (i = 0; i < 15; i++) {
		x = 1 + (int) (26.0 * rand() / (RAND_MAX + 1.0));

		if (1 + (int) (2.0 * rand() / (RAND_MAX + 1.0)) == 2)
			x += 32;

		buf[i] = x + 64;
	}

	buf[i] = 0;

	config_set_str(OPENNAP->cfg, "main/username", buf);
}

static BOOL gift_cb_start(Protocol *p)
{
	if (!(OPENNAP->cfg = config_load())) {
		GIFT_ERROR(("Can't load OpenNap configuration!"));
		return FALSE;
	}

	set_username();

	OPENNAP->nodelist = opn_nodelist_new();

	/* opn_nodelist_load(OPENNAP->nodelist); */
	opn_nodelist_refresh(OPENNAP->nodelist);
	
	return TRUE;
}

static void gift_cb_destroy(Protocol *p)
{
	List *l;
	
	if (!OPENNAP)
		return;

	/* opn_nodelist_save(OPENNAP->nodelist); */

	config_free(OPENNAP->cfg);

	for (l = OPENNAP->sessions; l; l = l->next)
		opn_session_free((OpnSession *) l->data);
	
	opn_nodelist_free(OPENNAP->nodelist);
	/* FIXME free OPENNAP->searches */
	free(OPENNAP);
}

static void setup_callbacks(Protocol *p)
{
	p->start = gift_cb_start;
	p->destroy = gift_cb_destroy;

	p->search = gift_cb_search;

	p->download_start = gift_cb_download_start;
	p->download_stop = gift_cb_download_stop;
	p->source_remove = gift_cb_source_remove;

#if 0
	p->share_hide = gift_cb_share_hide;
	p->share_show = gift_cb_share_show;
#endif
	
	p->stats = gift_cb_stats;
}

BOOL OpenNap_init(Protocol *p)
{
	OpnPlugin *plugin;
	
	if (protocol_compat(LIBGIFTPROTO_VERSION))
		return FALSE;

	/* tell your debugger to break here.
	 * only works on x86 and GLibC 2
	 */
	__asm__ __volatile__ ("int $03");
	 
	opn_proto = p;

	if (!(plugin = malloc(sizeof(OpnPlugin))))
		return FALSE;

	memset(plugin, 0, sizeof(OpnPlugin));

	p->udata = plugin;

	setup_callbacks(p);
	
	return TRUE;
}


