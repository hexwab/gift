# $Id: Makefile.am,v 1.18 2004/01/01 01:38:23 hipnod Exp $
###############################################################################

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
