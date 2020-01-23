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

#include "opn_opennap.h"
#include "opn_search.h"

#define OPN_MAX_SEARCH_RESULTS 512

OpnSearch *opn_search_new()
{
	OpnSearch *search;

	if (!(search = malloc(sizeof(OpnSearch))))
		return NULL;

	memset(search, 0, sizeof(OpnSearch));

	opn_search_ref(search);
	
	return search;
}

void opn_search_free(OpnSearch *search)
{
	if (!search)
		return;
	
	opn_proto->search_complete(opn_proto, search->event);
	free(search);
}

uint32_t opn_search_ref(OpnSearch *search)
{
	if (!search)
		return 0;

	return ++search->ref;
}

uint32_t opn_search_unref(OpnSearch *search)
{
	if (!search)
		return 0;

	if (!--search->ref) {
		OPENNAP->searches = list_remove(OPENNAP->searches, search);
		opn_search_free(search);
		return 0;
	} else
		return search->ref;
}

static BOOL file_cmp_query(char *file, char *query)
{
	char *tmp, *token;

	assert(query);
	assert(file);

	if (string_isempty(query))
		return TRUE;

	if (!strchr(query, ' '))
		return (strcasestr(file, query) != NULL);
	
	tmp = strdup(query);

	while ((token = string_sep(&tmp, " ")))
		if (strcasestr(file, token)) {
			free(tmp);
			return FALSE;
		}

	free(tmp);
	return TRUE;
}

OpnSearch *opn_search_find(char *file)
{
	OpnSearch *search;
	List *l;
	BOOL match_query, match_excl;

	assert(file);

	for (l = OPENNAP->searches; l; l = l->next) {
		search = (OpnSearch *) l->data;

		printf("%s|%s [%s]\n", search->query, search->exclude, file);
		match_query = file_cmp_query(file, search->query);
		match_excl = file_cmp_query(file, search->exclude);
		
		if (match_query && !match_excl)
			return search;
	}

	printf("sux!!!\n");
	return NULL;
}

BOOL gift_cb_search(Protocol *p, IFEvent *event, char *query, char *exclude,
                   char *realm, Dataset *meta)
{
	OpnSearch *search;
	OpnPacket *packet;
	OpnSession *session;
	List *l;
	char buf[256];

	if (!opn_is_connected || !(search = opn_search_new()))
		return FALSE;

	OPENNAP->searches = list_prepend(OPENNAP->searches, search);

	snprintf(search->query, sizeof(search->query), "%s", query);
	snprintf(search->exclude, sizeof(search->exclude), "%s", exclude);
	search->event = event;
	
	for (l = OPENNAP->sessions; l; l = l->next) {
		session = (OpnSession *) l->data;
		
		if (session->state != OPN_SESSION_STATE_CONNECTED)
			continue;

		snprintf(buf, sizeof(buf),
		        "MAX_RESULTS %i FILENAME CONTAINS \"%s\"",
				OPN_MAX_SEARCH_RESULTS, query);

		if (!(packet = opn_packet_new(OPN_CMD_SEARCH, buf, strlen(buf))))
			continue;

		opn_packet_send(packet, session->con);
		opn_packet_free(packet);

		opn_search_ref(search);
	}

	return (opn_search_unref(search) > 0);
}

