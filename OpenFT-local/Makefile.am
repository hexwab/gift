# $Id: Makefile.am,v 1.51 2003/12/26 21:38:05 jasta Exp $
###############################################################################

SUBDIRS = \
	data \
	etc \
	m4 \
	src

EXTRA_DIST = \
	Make.msvc \
	Makefile.msvc \
	autogen.sh \
	openft.spec

###############################################################################

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
