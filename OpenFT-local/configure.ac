###############################################################################
##
## $Id: configure.ac,v 1.28 2004/11/30 17:31:18 hexwab Exp $
##
## Modified from giFT's original configure.ac.  See authorship information
## there.
##
## This program is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by the
## Free Software Foundation; either version 2, or (at your option) any
## later version.
##
## This program is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
###############################################################################

dnl ###########################################################################
dnl Initialize autoconf...
dnl ###########################################################################

AC_PREREQ(2.53)

AC_INIT(gift-openft, 0.2.1.5)
AC_CONFIG_SRCDIR([src/ft_openft.h])

# TODO: parse from $VERSION?
OPENFT_MAJOR=0
OPENFT_MINOR=2
OPENFT_MICRO=1
OPENFT_REV=5

AC_DEFINE_UNQUOTED(OPENFT_MAJOR, $OPENFT_MAJOR, [Major])
AC_DEFINE_UNQUOTED(OPENFT_MINOR, $OPENFT_MINOR, [Minor])
AC_DEFINE_UNQUOTED(OPENFT_MICRO, $OPENFT_MICRO, [Micro])
AC_DEFINE_UNQUOTED(OPENFT_REV,   $OPENFT_REV,   [Revision])

dnl ###########################################################################
dnl Initialize automake...
dnl ###########################################################################

AM_INIT_AUTOMAKE([dist-bzip2])
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

dnl ###########################################################################
dnl Initialize autoheader...
dnl ###########################################################################

AH_VERBATIM([USE_ZLIB],  [#undef USE_ZLIB])
AH_VERBATIM([USE_LIBDB], [#undef USE_LIBDB])

dnl ###########################################################################
dnl Check for the necessary build tools and configure libtool
dnl ###########################################################################

AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PROG_INSTALL
AC_PATH_PROG(SED, sed)

AC_CHECK_HEADERS(sys/resource.h)

AM_DISABLE_STATIC
AC_PROG_LIBTOOL

PLUGIN_LDFLAGS="-module -avoid-version"
AC_SUBST(PLUGIN_LDFLAGS)

# work around a bug where need_version is set to no by the above
# -avoid-version call when this is not supported on OpenBSD (or am I
# completely wrong about this?)
OPENBSD_LIBTOOL_WORKAROUND

dnl ###########################################################################
dnl Handle the configure switches...
dnl ###########################################################################

AC_ARG_ENABLE(libdb,       [  --disable-libdb         do not use libdb],[use_libdb=$enableval],[use_libdb=yes])
AM_CONDITIONAL(LIBDB,       test x$use_libdb       = xyes)

dnl ###########################################################################
dnl Overrides...
dnl ###########################################################################

CFLAGS="$CFLAGS -Wall"

if test x$USE_MAINTAINER_MODE = xyes; then
  CFLAGS="$CFLAGS -g3 -O0"
fi

dnl ###########################################################################
dnl Setup prefix and install path
dnl ###########################################################################

GIFT_PLUGIN_PREFIX

dnl ###########################################################################
dnl Test for libgift and libgiftproto...
dnl ###########################################################################

GIFT_PLUGIN_PKGCONFIG(OPENFT, 0.11.8, 0.12.0)
AC_SUBST(OPENFT_CFLAGS)
AC_SUBST(OPENFT_LIBS)

dnl ###########################################################################
dnl Check for zlib...
dnl ###########################################################################

# set the minimum ZLIB_VER we're willing to link against...
ZLIB_VER=1.1.4

# for some reason Darwin has a 1.1.3 version with the 1.1.4 security fix
# applied backwards...
case "${host}" in
  *-*-darwin* )
    ZLIB_VER=1.1.3
  ;;
esac

dnl Default to compiler & linker defaults for files & libraries.
OPT_ZLIB=
AC_ARG_WITH(zlib,dnl
[  --with-zlib[=DIR]         where to look for ZLIB [compiler/linker default paths]
                          DIR points to the ZLIB installation prefix [/usr/local]],
 OPT_ZLIB=$withval,
)

#AC_MSG_CHECKING(where to look for ZLIB)
if test X"$OPT_ZLIB" = Xno
then
  zlib_ok=no
else
  ZLIB_DIRS="$OPT_ZLIB /usr /usr/local /sw"
  for ZLIB_DIR in $ZLIB_DIRS;
  do
    LIBS_SAVE="$LIBS"
    CPPFLAGS_SAVE="$CPPFLAGS"
    LIBS="$LIBS -L${ZLIB_DIR}/lib"
    CPPFLAGS="$CPPFLAGS -I${ZLIB_DIR}/include"
    AC_CACHE_CHECK(
      [for zlib version ${ZLIB_VER}+ in ${ZLIB_DIR}],
      zlib_ok,
      AC_TRY_RUN(
        [#include <zlib.h>
         #include <string.h>
         void main() {
           exit(strcmp(ZLIB_VERSION, "${ZLIB_VER}") < 0);
         }
         ],
        [zlib_ok=yes],
        [zlib_ok=no],
        [zlib_ok=yes]))

    if test "$zlib_ok" != "no"; then
      AC_CHECK_FUNC(gzread, , AC_CHECK_LIB(z, gzread))
      AC_DEFINE(USE_ZLIB)
      AC_SUBST(USE_ZLIB)
      break
    fi
    LIBS="$LIBS_SAVE"
    CPPFLAGS="$CPPFLAGS_SAVE"
  done

  if test "$zlib_ok" = "no"; then
    AC_MSG_ERROR([
NOTE: The zlib compression library version ${ZLIB_VER} or greater was not found
on your system.

If zlib ${ZLIB_VER}+ is not installed, install it.
    ])
  fi
fi

dnl ###########################################################################
dnl Check for the Berkeley Database 3.x or (preferrably) 4.x...
dnl ###########################################################################

found_dbh=no
if test x$use_libdb = xyes; then
dnl ########################################
dnl # first of all we check for latest db4 #
dnl # debian uses /usr/include/db.h        #
dnl # redhat is in /usr/include/db3/db.h   #
dnl # suppose futher versions in db4/db.h  #
dnl ########################################
  AC_CHECK_HEADERS(db4/db.h db.h db3/db.h,
            [ found_dbh=yes
                break  ],
            [ found_dbh=no ])
fi
if (test x$use_libdb = xyes) && ( test x$found_dbh = xyes); then
dnl From http://www.sleepycat.com/update/3.2.9/if.3.2.9.html
dnl ChangeLog:
dnl 9. Remove the app_private field from the DBT structure. Add app_private
dnl fields to the DB and DB_ENV structures. [#2862]
dnl @mator -> was compiling with libdb support on version 3.1.17
dnl got the following error:
dnl share_host.c: In function `cleanup_db':
dnl share_host.c:76: structure has no member named `app_private'
dnl Lucky you Debian 3.0 users, you have been packaged with right version ;-)
dnl And someone should really check in AC_TRY_RUN for at least version 3.2.9
# AC_MSG_NOTICE([Checking for min version 3.2.9...])
    for dbname in db db3 db-3.0 db4 db-4.0 db-4; do
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      LIBS="$LIBS -l$dbname"
      AC_CACHE_CHECK([for libdb (-l$dbname)], found_libdb,
        AC_TRY_RUN(
          [
        #ifdef HAVE_DB4_DB_H
        #include <db4/db.h>
        #endif
        #ifdef HAVE_DB_H
        #include <db.h>
        #endif
        #ifdef HAVE_DB3_DB_H
        #include <db3/db.h>
        #endif
        #include <stdio.h>
        int main (void)
        {
        DB *db;
        db_create (&db, NULL, 0);
        if (DB_VERSION_MAJOR < 3) {
            fprintf(stderr, "giFT requires libdb version 3 or above\nYou have %s\n",
            DB_VERSION_STRING);
            return(1);
        }
        return (0);
        }
          ],
          [
            LIBDB_CFLAGS=""
            LIBDB_LIBS="-l$dbname"
            AC_SUBST(LIBDB_CFLAGS)
            AC_SUBST(LIBDB_LIBS)
            AC_DEFINE(USE_LIBDB)
            found_libdb=yes
          ],
          [ found_libdb=no ]
        )
      )
      CFLAGS="$ac_save_CFLAGS"
      LIBS="$ac_save_LIBS"
      if test x$found_libdb == xyes; then break; fi
    done
  use_libdb=$found_libdb
else
  use_libdb=no
# AC_MSG_NOTICE([No db.h found, will be unable to run as search/index node...])
fi

dnl ###########################################################################
dnl Configure isntallation paths...
dnl ###########################################################################

GIFT_PLUGIN_INSTALL_PATH

dnl ###########################################################################
dnl Finish generation of the build environment
dnl ###########################################################################

AC_CONFIG_FILES([
Makefile
m4/Makefile
src/Makefile
src/proto/Makefile
data/Makefile
etc/Makefile
openft.spec
])

AC_OUTPUT

dnl ###########################################################################
dnl Print summary to the user...
dnl ###########################################################################

echo
echo "$PACKAGE_NAME $VERSION"
echo
echo "-------------------- core ---"
echo
echo "libgift.................: $libgift_version"
echo "libgiftproto............: $libgift_version"
echo
echo "-------------- extensions ---"
echo
echo "use zlib................: yes"
echo "use libdb...............: $use_libdb"
echo
echo "-----------------------------"
echo
echo "$PACKAGE_NAME will be installed in $plugindir"
echo
