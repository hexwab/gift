# $Id: Makefile.am,v 1.5 2004/01/29 11:36:03 hipnod Exp $

AM_CFLAGS = \
	$(GNUTELLA_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)/src

noinst_LTLIBRARIES = \
	libgt_io.la

libgt_io_la_SOURCES = \
	io_buf.c          io_buf.h \
	rx_inflate.c      rx_inflate.h \
	rx_layer.c        rx_layer.h \
	rx_link.c         rx_link.h \
	rx_packet.c       rx_packet.h \
	rx_stack.c        rx_stack.h \
	tx_layer.c        tx_layer.h \
	tx_link.c         tx_link.h \
	tx_deflate.c \
	tx_packet.c \
	tx_stack.c        tx_stack.h

