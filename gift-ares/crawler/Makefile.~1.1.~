
CFLAGS=-g -Wall -I../lib -DDEBUG #-DHEAVY_DEBUG
#CFLAGS=-O2 -Wall -I../lib

LDFLAGS=-L./ -levent -lm -lz

LIBARES_DIR = ../lib/

LIBARES_OBJECTS = \
	$(LIBARES_DIR)as_event.o \
	$(LIBARES_DIR)as_strobj.o \
	$(LIBARES_DIR)as_list.o \
	$(LIBARES_DIR)as_tcp.o \
	$(LIBARES_DIR)as_log.o \
	$(LIBARES_DIR)as_parse.o \
	$(LIBARES_DIR)as_hashtable.o \
	$(LIBARES_DIR)as_file.o \
	$(LIBARES_DIR)as_ares.o \
	$(LIBARES_DIR)as_crypt.o \
	$(LIBARES_DIR)as_config.o \
	$(LIBARES_DIR)as_packet.o \
	$(LIBARES_DIR)as_sha1.o \
	$(LIBARES_DIR)as_encoding.o \
	$(LIBARES_DIR)as_hash.o \
	$(LIBARES_DIR)as_session.o \
	$(LIBARES_DIR)as_session_man.o \
	$(LIBARES_DIR)as_node.o \
	$(LIBARES_DIR)as_node_man.o \
	$(LIBARES_DIR)as_meta.o \
	$(LIBARES_DIR)as_netinfo.o \
	$(LIBARES_DIR)as_source.o \
	$(LIBARES_DIR)as_tokenize.o \
	$(LIBARES_DIR)as_search_result.o \
	$(LIBARES_DIR)as_search.o \
	$(LIBARES_DIR)as_search_man.o \
	$(LIBARES_DIR)as_http_header.o \
	$(LIBARES_DIR)as_http_client.o \
	$(LIBARES_DIR)as_http_server.o \
	$(LIBARES_DIR)as_download_chunk.o \
	$(LIBARES_DIR)as_download_conn.o \
	$(LIBARES_DIR)as_download_state.o \
	$(LIBARES_DIR)as_download_man.o \
	$(LIBARES_DIR)as_download.o \
	$(LIBARES_DIR)as_incoming.o \
	$(LIBARES_DIR)as_share.o \
	$(LIBARES_DIR)as_share_man.o \
	$(LIBARES_DIR)as_push.o \
	$(LIBARES_DIR)as_push_man.o \
	$(LIBARES_DIR)as_upload.o \
	$(LIBARES_DIR)as_upload_man.o \
	$(LIBARES_DIR)as_util.o


OBJECTS= \
	main.o

all: ascrawl

libares.a: libares.a($(LIBARES_OBJECTS))

ascrawl: libares.a $(OBJECTS) 
	$(CC) $+ -o $@ $(LDFLAGS) ./libares.a

clean:
	rm -f ascrawl libares.a $(OBJECTS) $(LIBARES_OBJECTS)
