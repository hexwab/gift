#!/bin/sh
# $Id: autogen.mingw.sh,v 1.1 2002/03/23 05:50:03 rossta Exp $

CC=i586-mingw32msvc-gcc \
./autogen.sh --host=i586-mingw32msvc --target=i586-mingw32msvc \
             --build=i386-linux --disable-gtk-client --disable-libdl \
             --enable-maintainer-mode $*
