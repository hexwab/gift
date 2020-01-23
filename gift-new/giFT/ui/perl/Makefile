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
top_srcdir = ../..

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

top_builddir = ../..

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
GIFT_CFLAGS = 
GIFT_LDFLAGS =  -rdynamic
GIFT_LIBS = -lnsl  -ldl
GTKCFLAGS = 
GTKLIBS = 
GTK_CFLAGS = 
GTK_CONFIG = 
GTK_LIBS = 
LIBTOOL = $(SHELL) $(top_builddir)/libtool
LN_S = ln -s
MAKEINFO = makeinfo
OBJDUMP = @OBJDUMP@
OBJEXT = o
PACKAGE = giFT
PERL_CFLAGS = -DDEBIAN -fno-strict-aliasing -I/usr/local/include -D_LARGEFILE_SOURCE  -I/usr/lib/perl/5.6.1/CORE
PERL_LIBS = -rdynamic -L/usr/local/lib /usr/lib/perl/5.6.1/auto/DynaLoader/DynaLoader.a -L/usr/lib/perl/5.6.1/CORE -lperl -ldl -lm -lcrypt
RANLIB = ranlib
STRIP = strip
USE_DLOPEN = 
USE_ZLIB = 
VERSION = 0.10.0
giftconfdir = /usr/local/etc/giFT
giftdatadir = /usr/local/share/giFT
giftperldir = /usr/local/bin
perlpath = /usr/bin/perl
plugindir = /usr/local/lib/giFT
prefix = /usr/local
sedpath = /bin/sed

EXTRA_DIST = giFT-perl

giFTperldir = $(giftperldir)
giFTperl_SCRIPTS = giFT-shell
mkinstalldirs = $(SHELL) $(top_srcdir)/mkinstalldirs
CONFIG_HEADER = ../../config.h
CONFIG_CLEAN_FILES = 
SCRIPTS =  $(giFTperl_SCRIPTS)

DIST_COMMON =  README Makefile.am Makefile.in


DISTFILES = $(DIST_COMMON) $(SOURCES) $(HEADERS) $(TEXINFOS) $(EXTRA_DIST)

TAR = tar
GZIP_ENV = --best
all: all-redirect
.SUFFIXES:
$(srcdir)/Makefile.in: Makefile.am $(top_srcdir)/configure.ac $(ACLOCAL_M4) 
	cd $(top_srcdir) && $(AUTOMAKE) --gnu ui/perl/Makefile

Makefile: $(srcdir)/Makefile.in  $(top_builddir)/config.status $(BUILT_SOURCES)
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= $(SHELL) ./config.status


install-giFTperlSCRIPTS: $(giFTperl_SCRIPTS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(giFTperldir)
	@list='$(giFTperl_SCRIPTS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(INSTALL_SCRIPT) $$p $(DESTDIR)$(giFTperldir)/`echo $$p|sed '$(transform)'`"; \
	    $(INSTALL_SCRIPT) $$p $(DESTDIR)$(giFTperldir)/`echo $$p|sed '$(transform)'`; \
	  else if test -f $(srcdir)/$$p; then \
	    echo " $(INSTALL_SCRIPT) $(srcdir)/$$p $(DESTDIR)$(giFTperldir)/`echo $$p|sed '$(transform)'`"; \
	    $(INSTALL_SCRIPT) $(srcdir)/$$p $(DESTDIR)$(giFTperldir)/`echo $$p|sed '$(transform)'`; \
	  else :; fi; fi; \
	done

uninstall-giFTperlSCRIPTS:
	@$(NORMAL_UNINSTALL)
	list='$(giFTperl_SCRIPTS)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(giFTperldir)/`echo $$p|sed '$(transform)'`; \
	done
tags: TAGS
TAGS:


distdir = $(top_builddir)/$(PACKAGE)-$(VERSION)/$(subdir)

subdir = ui/perl

distdir: $(DISTFILES)
	here=`cd $(top_builddir) && pwd`; \
	top_distdir=`cd $(top_distdir) && pwd`; \
	distdir=`cd $(distdir) && pwd`; \
	cd $(top_srcdir) \
	  && $(AUTOMAKE) --include-deps --build-dir=$$here --srcdir-name=$(top_srcdir) --output-dir=$$top_distdir --gnu ui/perl/Makefile
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

install-data-am: install-giFTperlSCRIPTS
install-data: install-data-am

install-am: all-am
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am
install: install-am
uninstall-am: uninstall-giFTperlSCRIPTS
uninstall: uninstall-am
all-am: Makefile $(SCRIPTS)
all-redirect: all-am
install-strip:
	$(MAKE) $(AM_MAKEFLAGS) AM_INSTALL_PROGRAM_FLAGS=-s install
installdirs:
	$(mkinstalldirs)  $(DESTDIR)$(giFTperldir)


mostlyclean-generic:

clean-generic:

distclean-generic:
	-rm -f Makefile $(CONFIG_CLEAN_FILES)
	-rm -f config.cache config.log stamp-h stamp-h[0-9]*

maintainer-clean-generic:
mostlyclean-am:  mostlyclean-generic

mostlyclean: mostlyclean-am

clean-am:  clean-generic mostlyclean-am

clean: clean-am

distclean-am:  distclean-generic clean-am
	-rm -f libtool

distclean: distclean-am

maintainer-clean-am:  maintainer-clean-generic distclean-am
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."

maintainer-clean: maintainer-clean-am

.PHONY: uninstall-giFTperlSCRIPTS install-giFTperlSCRIPTS tags distdir \
info-am info dvi-am dvi check check-am installcheck-am installcheck \
install-exec-am install-exec install-data-am install-data install-am \
install uninstall-am uninstall all-redirect all-am all installdirs \
mostlyclean-generic distclean-generic clean-generic \
maintainer-clean-generic clean mostlyclean distclean maintainer-clean


# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
