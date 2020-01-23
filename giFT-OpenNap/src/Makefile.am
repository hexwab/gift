AM_CFLAGS = $(CFLAGS)

plugin_LTLIBRARIES = libOpenNap.la

libOpenNap_la_SOURCES = \
	opn_opennap.c opn_opennap.h \
	opn_packet.c opn_packet.h \
	opn_session.c opn_session.h \
	opn_node.c opn_node.h \
	opn_protocol.c opn_protocol.h \
	opn_protocol_handlers.c opn_protocol_handlers.h \
	opn_search.c opn_search.h \
	opn_download.c opn_download.h \
	opn_url.c opn_url.h \
	opn_share.c opn_share.h

INCLUDES = @GIFT_OPENNAP_CFLAGS@
libOpenNap_la_LIBADD = @GIFT_OPENNAP_LIBS@ -lgiftproto

