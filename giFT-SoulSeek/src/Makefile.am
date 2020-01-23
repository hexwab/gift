AM_CFLAGS = $(SL_CFLAGS)

plugin_LTLIBRARIES = libSoulSeek.la

libSoulSeek_la_SOURCES = \
sl_download.c     sl_download.h \
sl_meta.c         sl_meta.h \
sl_node.c         sl_node.h \
sl_packet.c       sl_packet.h \
sl_peer.c         sl_peer.h \
sl_search.c       sl_search.h \
sl_session.c      sl_session.h \
sl_share.c        sl_share.h \
sl_soulseek.c     sl_soulseek.h \
sl_stats.c        sl_stats.h \
sl_string.c       sl_string.h \
sl_utils.c        sl_utils.h \
sl_filelist.c

libSoulSeek_la_LIBADD = $(SL_LIBS)
libSoulSeek_la_LDFLAGS = $(PLUGIN_LDFLAGS)
