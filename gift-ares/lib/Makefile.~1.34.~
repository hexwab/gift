#CFLAGS=-O2 -Wall -DNDEBUG
CFLAGS= -g3 -Wall -DDEBUG #-DHEAVY_DEBUG

# stuff also available in libgift
GIFT_OBJECTS = \
	as_event.o \
	as_strobj.o \
	as_list.o \
	as_tcp.o \
	as_parse.o

OBJECTS = \
	as_hashtable.o \
	as_file.o \
	as_log.o \
	as_ares.o \
	as_crypt.o \
	as_config.o \
	as_packet.o \
	as_sha1.o \
	as_encoding.o \
	as_hash.o \
	as_session.o \
	as_session_man.o \
	as_node.o \
	as_node_man.o \
	as_meta.o \
	as_netinfo.o \
	as_source.o \
	as_tokenize.o \
	as_search_result.o \
	as_search.o \
	as_search_man.o \
	as_http_header.o \
	as_http_client.o \
	as_http_server.o \
	as_download_chunk.o \
	as_download_conn.o \
	as_download_state.o \
	as_download_man.o \
	as_download.o \
	as_incoming.o \
	as_share.o \
	as_share_man.o \
	as_push.o \
	as_push_man.o \
	as_upload.o \
	as_upload_man.o \
	as_util.o

all: libares.a

libares.a: libares.a($(OBJECTS) $(GIFT_OBJECTS))

libaresgift-temp: libaresgift.a($(OBJECTS))

libaresgift.a:
	make libaresgift-temp CFLAGS="$(CFLAGS) -DGIFT_PLUGIN"

clean:
	rm -f libares.a libaresgift.a $(OBJECTS) $(GIFT_OBJECTS)

.PHONY: libares libaresgift.a libaresgift-temp
