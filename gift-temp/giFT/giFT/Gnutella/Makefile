# Makefile.in generated automatically by automake 1.4-p4 from Makefile.am

# Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.


SHELL = /bin/sh

srcdir = .
top_srcdir = ..

prefix = /usr/local
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
libexecdir = ${exec_prefix}/libexec
datadir = ${prefix}/share
sysconfdir = ${prefix}/etc
sharedstatedir = ${prefix}/com
localstatedir = ${prefix}/var
libdir = ${exec_prefix}/lib
infodir = ${prefix}/info
mandir = ${prefix}/man
includedir = ${prefix}/include
oldincludedir = /usr/include

DESTDIR =

pkgdatadir = $(datadir)/giFT
pkglibdir = $(libdir)/giFT
pkgincludedir = $(includedir)/giFT

top_builddir = ..

ACLOCAL = aclocal
AUTOCONF = autoconf
AUTOMAKE = automake
AUTOHEADER = autoheader

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL} $(AM_INSTALL_PROGRAM_FLAGS)
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_SCRIPT = ${INSTALL}
transform = s,x,x,

NORMAL_INSTALL = :
PRE_INSTALL = :
POST_INSTALL = :
NORMAL_UNINSTALL = :
PRE_UNINSTALL = :
POST_UNINSTALL = :
host_alias = 
host_triplet = i686-pc-linux-gnu
AS = @AS@
CC = gcc
DLLTOOL = @DLLTOOL@
ECHO = echo
EXEEXT = 
GIFT_CFLAGS = -g -Wall 
GIFT_LDFLAGS =  -rdynamic
GIFT_LIBS = -lnsl  -lltdl
ID3LIB_CFLAGS = 
ID3LIB_LIBS = 
IMAGEMAGICK_CFLAGS = 
IMAGEMAGICK_LDFLAGS = 
IMAGEMAGICK_LIBS = 
LIBDB_CFLAGS = 
LIBDB_LIBS = -ldb
LIBTOOL = $(SHELL) $(top_builddir)/libtool
LN_S = ln -s
MAKEINFO = makeinfo
OBJDUMP = @OBJDUMP@
OBJEXT = o
PACKAGE = giFT
PERL_CFLAGS = 
PERL_LIBS = 
PLUGIN_GNUTELLA = 
PLUGIN_OPENFT = 
RANLIB = ranlib
STRIP = strip
USE_LTDL = 
USE_ZLIB = 
VERSION = 0.10.0
VORBISENC_LIBS = -L/usr/local/lib -lvorbisenc
VORBISFILE_LIBS = -L/usr/local/lib -lvorbisfile
VORBIS_CFLAGS = -I/usr/local/include
VORBIS_LIBS = -L/usr/local/lib -lvorbis -lm
giftdatadir = /usr/local/share/giFT
giftperldir = /usr/local/bin
perlpath = 
plugindir = /usr/local/lib/giFT
prefix = /usr/local
sedpath = 

AM_CFLAGS = $(GIFT_CFLAGS)

INCLUDES = -I$(top_srcdir)/lib 	   -I$(top_srcdir)/src 	   -I$(top_srcdir)/plugin


# libdir = $(plugindir)

#
# The rest of this file is conditional on whether we're 
# compiling the gnutella plugin at all
#

#plugin_LTLIBRARIES = libGnutella.la

#libGnutella_la_SOURCES = 	gt_guid.c          gt_guid.h 	gt_accept.c        gt_accept.h 	gt_connect.c       gt_connect.h 	gt_node.c          gt_node.h 	gt_packet.c        gt_packet.h 	gt_protocol.c      gt_protocol.h 	gt_netorg.c        gt_netorg.h 	gt_utils.c         gt_utils.h 	gt_xfer.c          gt_xfer.h 	gt_query_route.c   gt_query_route.h 	gt_search.c        gt_search.h 	gt_search_exec.c   gt_search_exec.h 	gt_share_file.c    gt_share_file.h 	gt_share.c         gt_share.h 	gt_stats.c         gt_stats.h 	gt_web_cache.c     gt_web_cache.h 	ft_http_client.c   ft_http_client.h 	ft_http_server.c   ft_http_server.h 	ft_xfer.c          ft_xfer.h 	file_cache.c       file_cache.h 	sha1.c             sha1.h 	html.c             html.h 	http_request.c     http_request.h 	gt_gnutella.c      gt_gnutella.h 	http.c             http.h

# _BIG_ hack
# libGnutella_la_LDFLAGS = -module -avoid-version -L$(top_builddir)/lib/.libs

#libGnutella_la_LIBADD = 	$(top_builddir)/lib/libgiFT.la 	$(top_builddir)/plugin/libgiFTproto.la
mkinstalldirs = $(SHELL) $(top_srcdir)/mkinstalldirs
CONFIG_HEADER = ../config.h
CONFIG_CLEAN_FILES = 
LTLIBRARIES =  $(plugin_LTLIBRARIES)


DEFS = -DHAVE_CONFIG_H -I. -I$(srcdir) -I..
CPPFLAGS =  -I/usr/include
LDFLAGS = 
LIBS = -lz -lnsl  -L/usr/lib
libGnutella_la_LDFLAGS = 
#libGnutella_la_DEPENDENCIES =  \
#$(top_builddir)/lib/libgiFT.la \
#$(top_builddir)/plugin/libgiFTproto.la
#libGnutella_la_OBJECTS =  gt_guid.lo gt_accept.lo \
#gt_connect.lo gt_node.lo gt_packet.lo gt_protocol.lo \
#gt_netorg.lo gt_utils.lo gt_xfer.lo gt_query_route.lo \
#gt_search.lo gt_search_exec.lo gt_share_file.lo \
#gt_share.lo gt_stats.lo gt_web_cache.lo \
#ft_http_client.lo ft_http_server.lo ft_xfer.lo \
#file_cache.lo sha1.lo html.lo http_request.lo \
#gt_gnutella.lo http.lo
CFLAGS = -g -Wall
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
LTCOMPILE = $(LIBTOOL) --mode=compile $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(LIBTOOL) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@
DIST_COMMON =  README Makefile.am Makefile.in


DISTFILES = $(DIST_COMMON) $(SOURCES) $(HEADERS) $(TEXINFOS) $(EXTRA_DIST)

TAR = tar
GZIP_ENV = --best
DEP_FILES =  .deps/file_cache.P .deps/ft_http_client.P \
.deps/ft_http_server.P .deps/ft_xfer.P .deps/gt_accept.P \
.deps/gt_connect.P .deps/gt_gnutella.P .deps/gt_guid.P \
.deps/gt_netorg.P .deps/gt_node.P .deps/gt_packet.P .deps/gt_protocol.P \
.deps/gt_query_route.P .deps/gt_search.P .deps/gt_search_exec.P \
.deps/gt_share.P .deps/gt_share_file.P .deps/gt_stats.P \
.deps/gt_utils.P .deps/gt_web_cache.P .deps/gt_xfer.P .deps/html.P \
.deps/http.P .deps/http_request.P .deps/sha1.P
SOURCES = $(libGnutella_la_SOURCES)
OBJECTS = $(libGnutella_la_OBJECTS)

all: all-redirect
.SUFFIXES:
.SUFFIXES: .S .c .lo .o .obj .s
$(srcdir)/Makefile.in: Makefile.am $(top_srcdir)/configure.ac $(ACLOCAL_M4) 
	cd $(top_srcdir) && $(AUTOMAKE) --gnu Gnutella/Makefile

Makefile: $(srcdir)/Makefile.in  $(top_builddir)/config.status $(BUILT_SOURCES)
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= $(SHELL) ./config.status


mostlyclean-pluginLTLIBRARIES:

clean-pluginLTLIBRARIES:
	-test -z "$(plugin_LTLIBRARIES)" || rm -f $(plugin_LTLIBRARIES)

distclean-pluginLTLIBRARIES:

maintainer-clean-pluginLTLIBRARIES:

install-pluginLTLIBRARIES: $(plugin_LTLIBRARIES)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(plugindir)
	@list='$(plugin_LTLIBRARIES)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo "$(LIBTOOL)  --mode=install $(INSTALL) $$p $(DESTDIR)$(plugindir)/$$p"; \
	    $(LIBTOOL)  --mode=install $(INSTALL) $$p $(DESTDIR)$(plugindir)/$$p; \
	  else :; fi; \
	done

uninstall-pluginLTLIBRARIES:
	@$(NORMAL_UNINSTALL)
	list='$(plugin_LTLIBRARIES)'; for p in $$list; do \
	  $(LIBTOOL)  --mode=uninstall rm -f $(DESTDIR)$(plugindir)/$$p; \
	done

# FIXME: We should only use cygpath when building on Windows,
# and only if it is available.
.c.obj:
	$(COMPILE) -c `cygpath -w $<`

.s.o:
	$(COMPILE) -c $<

.S.o:
	$(COMPILE) -c $<

mostlyclean-compile:
	-rm -f *.o core *.core
	-rm -f *.$(OBJEXT)

clean-compile:

distclean-compile:
	-rm -f *.tab.c

maintainer-clean-compile:

.s.lo:
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

.S.lo:
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

mostlyclean-libtool:
	-rm -f *.lo

clean-libtool:
	-rm -rf .libs _libs

distclean-libtool:

maintainer-clean-libtool:

libGnutella.la: $(libGnutella_la_OBJECTS) $(libGnutella_la_DEPENDENCIES)
	$(LINK) -rpath $(plugindir) $(libGnutella_la_LDFLAGS) $(libGnutella_la_OBJECTS) $(libGnutella_la_LIBADD) $(LIBS)

tags: TAGS

ID: $(HEADERS) $(SOURCES) $(LISP)
	list='$(SOURCES) $(HEADERS)'; \
	unique=`for i in $$list; do echo $$i; done | \
	  awk '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	here=`pwd` && cd $(srcdir) \
	  && mkid -f$$here/ID $$unique $(LISP)

TAGS:  $(HEADERS) $(SOURCES)  $(TAGS_DEPENDENCIES) $(LISP)
	tags=; \
	here=`pwd`; \
	list='$(SOURCES) $(HEADERS)'; \
	unique=`for i in $$list; do echo $$i; done | \
	  awk '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	test -z "$(ETAGS_ARGS)$$unique$(LISP)$$tags" \
	  || (cd $(srcdir) && etags $(ETAGS_ARGS) $$tags  $$unique $(LISP) -o $$here/TAGS)

mostlyclean-tags:

clean-tags:

distclean-tags:
	-rm -f TAGS ID

maintainer-clean-tags:

distdir = $(top_builddir)/$(PACKAGE)-$(VERSION)/$(subdir)

subdir = Gnutella

distdir: $(DISTFILES)
	here=`cd $(top_builddir) && pwd`; \
	top_distdir=`cd $(top_distdir) && pwd`; \
	distdir=`cd $(distdir) && pwd`; \
	cd $(top_srcdir) \
	  && $(AUTOMAKE) --include-deps --build-dir=$$here --srcdir-name=$(top_srcdir) --output-dir=$$top_distdir --gnu Gnutella/Makefile
	@for file in $(DISTFILES); do \
	  d=$(srcdir); \
	  if test -d $$d/$$file; then \
	    cp -pr $$d/$$file $(distdir)/$$file; \
	  else \
	    test -f $(distdir)/$$file \
	    || ln $$d/$$file $(distdir)/$$file 2> /dev/null \
	    || cp -p $$d/$$file $(distdir)/$$file || :; \
	  fi; \
	done

DEPS_MAGIC := $(shell mkdir .deps > /dev/null 2>&1 || :)

-include $(DEP_FILES)

mostlyclean-depend:

clean-depend:

distclean-depend:
	-rm -rf .deps

maintainer-clean-depend:

%.o: %.c
	@echo '$(COMPILE) -c $<'; \
	$(COMPILE) -Wp,-MD,.deps/$(*F).pp -c $<
	@-cp .deps/$(*F).pp .deps/$(*F).P; \
	tr ' ' '\012' < .deps/$(*F).pp \
	  | sed -e 's/^\\$$//' -e '/^$$/ d' -e '/:$$/ d' -e 's/$$/ :/' \
	    >> .deps/$(*F).P; \
	rm .deps/$(*F).pp

%.lo: %.c
	@echo '$(LTCOMPILE) -c $<'; \
	$(LTCOMPILE) -Wp,-MD,.deps/$(*F).pp -c $<
	@-sed -e 's/^\([^:]*\)\.o[ 	]*:/\1.lo \1.o :/' \
	  < .deps/$(*F).pp > .deps/$(*F).P; \
	tr ' ' '\012' < .deps/$(*F).pp \
	  | sed -e 's/^\\$$//' -e '/^$$/ d' -e '/:$$/ d' -e 's/$$/ :/' \
	    >> .deps/$(*F).P; \
	rm -f .deps/$(*F).pp
info-am:
info: info-am
dvi-am:
dvi: dvi-am
check-am: all-am
check: check-am
installcheck-am:
installcheck: installcheck-am
install-exec-am:
install-exec: install-exec-am

install-data-am: install-pluginLTLIBRARIES
install-data: install-data-am

install-am: all-am
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am
install: install-am
uninstall-am: uninstall-pluginLTLIBRARIES
uninstall: uninstall-am
all-am: Makefile $(LTLIBRARIES)
all-redirect: all-am
install-strip:
	$(MAKE) $(AM_MAKEFLAGS) AM_INSTALL_PROGRAM_FLAGS=-s install
installdirs:
	$(mkinstalldirs)  $(DESTDIR)$(plugindir)


mostlyclean-generic:

clean-generic:

distclean-generic:
	-rm -f Makefile $(CONFIG_CLEAN_FILES)
	-rm -f config.cache config.log stamp-h stamp-h[0-9]*

maintainer-clean-generic:
mostlyclean-am:  mostlyclean-pluginLTLIBRARIES mostlyclean-compile \
		mostlyclean-libtool mostlyclean-tags mostlyclean-depend \
		mostlyclean-generic

mostlyclean: mostlyclean-am

clean-am:  clean-pluginLTLIBRARIES clean-compile clean-libtool \
		clean-tags clean-depend clean-generic mostlyclean-am

clean: clean-am

distclean-am:  distclean-pluginLTLIBRARIES distclean-compile \
		distclean-libtool distclean-tags distclean-depend \
		distclean-generic clean-am
	-rm -f libtool

distclean: distclean-am

maintainer-clean-am:  maintainer-clean-pluginLTLIBRARIES \
		maintainer-clean-compile maintainer-clean-libtool \
		maintainer-clean-tags maintainer-clean-depend \
		maintainer-clean-generic distclean-am
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."

maintainer-clean: maintainer-clean-am

.PHONY: mostlyclean-pluginLTLIBRARIES distclean-pluginLTLIBRARIES \
clean-pluginLTLIBRARIES maintainer-clean-pluginLTLIBRARIES \
uninstall-pluginLTLIBRARIES install-pluginLTLIBRARIES \
mostlyclean-compile distclean-compile clean-compile \
maintainer-clean-compile mostlyclean-libtool distclean-libtool \
clean-libtool maintainer-clean-libtool tags mostlyclean-tags \
distclean-tags clean-tags maintainer-clean-tags distdir \
mostlyclean-depend distclean-depend clean-depend \
maintainer-clean-depend info-am info dvi-am dvi check check-am \
installcheck-am installcheck install-exec-am install-exec \
install-data-am install-data install-am install uninstall-am uninstall \
all-redirect all-am all installdirs mostlyclean-generic \
distclean-generic clean-generic maintainer-clean-generic clean \
mostlyclean distclean maintainer-clean


# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
