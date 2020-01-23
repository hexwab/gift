# $Id: Makefile.am,v 1.12 2005/02/03 20:41:52 mkern Exp $

SUBDIRS = src data m4 win32

EXTRA_DIST = \
	fasttrack.spec \
	Make.msvc \
	Makefile.msvc

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
