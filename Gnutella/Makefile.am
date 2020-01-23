# $Id: Makefile.am,v 1.19 2005/01/04 14:35:11 mkern Exp $
###############################################################################

# Flags to pass to aclocal when it is run automatically by make
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
	m4 \
	data \
	src

EXTRA_DIST = \
	gnutella.spec \
	autogen.sh \
	Makefile.msvc \
	Make.msvc

###############################################################################

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
