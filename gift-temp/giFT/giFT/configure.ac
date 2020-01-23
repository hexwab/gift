# -*- sh -*-

dnl ###########################################################################
dnl Process this file with autoconf to produce a configure script.
dnl ###########################################################################

AC_PREREQ(2.52)

AC_INIT
AC_CONFIG_SRCDIR([src/main.c])

GIFT_MAJOR=0
GIFT_MINOR=10
GIFT_MICRO=0
GIFT_VERSION=$GIFT_MAJOR.$GIFT_MINOR.$GIFT_MICRO

VERSION=$GIFT_VERSION

AM_INIT_AUTOMAKE(giFT, $VERSION)
AM_CONFIG_HEADER(config.h)

# AM_MAINTAINER_MODE

dnl ###########################################################################
dnl acconfig.h
dnl ###########################################################################

AH_VERBATIM([BUILD_DATE], [
#undef BUILD_DATE
])
AH_VERBATIM([DATA_DIR], [
#undef DATA_DIR
])
AH_VERBATIM([PLUGIN_DIR], [
#undef PLUGIN_DIR
])
AH_VERBATIM([PLUGIN_OPENFT], [
#undef PLUGIN_OPENFT
])
AH_VERBATIM([PLUGIN_GNUTELLA], [
#undef PLUGIN_GNUTELLA
])
AH_VERBATIM([USE_LTDL], [
#undef USE_LTDL
])
AH_VERBATIM([USE_PERL], [
#undef USE_PERL
])
AH_VERBATIM([USE_ZLIB], [
#undef USE_ZLIB
])
AH_VERBATIM([USE_ID3LIB], [
#undef USE_ID3LIB
])
AH_VERBATIM([USE_LIBVORBIS], [
#undef USE_LIBVORBIS
])
AH_VERBATIM([USE_LIBDB], [
#undef USE_LIBDB
])
AH_VERBATIM([USE_IMAGEMAGICK], [
#undef USE_IMAGEMAGICK
])

dnl ###########################################################################
dnl Build information
dnl ###########################################################################

BUILD_DATE="`date`"
AC_DEFINE_UNQUOTED(BUILD_DATE, "$BUILD_DATE")

dnl ###########################################################################
dnl Checks for programs.
dnl ###########################################################################

AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PROG_INSTALL

AM_DISABLE_STATIC
AC_PROG_LIBTOOL

dnl ###########################################################################
dnl Checks for libraries.
dnl ###########################################################################

AC_CHECK_LIB(nsl, gethostbyname)
AC_CHECK_LIB(socket, socket)
AC_CHECK_LIB(mingwex, opendir)
AC_CHECK_LIB(bind, inet_ntoa)
AC_CHECK_LIB(be, openlog)

GIFT_CHECK_WINSOCK_LIBS

dnl ###########################################################################
dnl Checks for header files.
dnl ###########################################################################

AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(dirent.h \
                 fcntl.h \
                 getopt.h \
                 limits.h \
                 linux/limits.h \
                 sys/mman.h \
                 sys/time.h \
                 syslog.h \
                 signal.h \
                 unistd.h \
                 inttypes.h \
                 io.h)

dnl ###########################################################################
dnl Checks for typedefs, structures, and compiler characteristics.
dnl ###########################################################################

AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

m4_define([AC_GIFT_CHECK_TYPE],
          [AC_CHECK_TYPE([$1],,
            [AC_DEFINE_UNQUOTED([$1], [$2],
              [Define to '$2' if <sys/types.h> and <stdint.h> do not define.])],
            [#include <sys/types.h>
             #ifdef HAVE_STDINT_H
             #include <stdint.h>
             #endif
             #ifdef HAVE_INTTYPES_H
             #include <inttypes.h>
             #endif
            ])
          ])

AC_C_BIGENDIAN

AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(void *)

if test "${ac_cv_sizeof_int}" = "4" ; then
   AS_VAR_SET(ac_cv_gift_int32, int)
elif test "${ac_cv_sizeof_long}" = "4"; then
   AS_VAR_SET(ac_cv_gift_int32, long)
else
   AC_MSG_ERROR(No 32-bit datatype found - please report to #giFT on irc.openprojects.net
               )
fi

AC_GIFT_CHECK_TYPE(uint8_t, unsigned char)
AC_GIFT_CHECK_TYPE(int8_t, char)
AC_GIFT_CHECK_TYPE(uint16_t, unsigned short )
AC_GIFT_CHECK_TYPE(int16_t, short )
AC_GIFT_CHECK_TYPE(uint32_t, unsigned AS_VAR_GET(ac_cv_gift_int32) )
AC_GIFT_CHECK_TYPE(int32_t, AS_VAR_GET(ac_cv_gift_int32) )

AC_CHECK_TYPE(in_addr_t,,
              [AC_DEFINE_UNQUOTED(
                [in_addr_t], [uint32_t],
                [Define to 'uint32' if <netinet/in.h> or <sys/types.h> does not define.])],
              [#include <sys/types.h>
               #include <netinet/in.h>
              ])

AS_IF([test AS_VAR_GET(ac_cv_type_in_addr_t) = yes],
      [AC_DEFINE_UNQUOTED(
        [HAVE_IN_ADDR_T], [1],
        [Define to 1 if you have in_addr_t])],
      [])

AC_CHECK_TYPE(in_port_t,,
              [AC_DEFINE_UNQUOTED(
                [in_port_t], [uint16_t],
                [Define to 'uint16' if <netinet/in.h> or <sys/types.h> does not define.])],
              [#include <sys/types.h>
               #include <netinet/in.h>
              ])

AS_IF([test AS_VAR_GET(ac_cv_type_in_port_t) = yes],
      [AC_DEFINE_UNQUOTED(
        [HAVE_IN_PORT_T], [1],
        [Define to 1 if you have in_port_t])],
      [])

AC_CACHE_CHECK([for getopt], ac_cv_func_getopt,
                 [AC_LINK_IFELSE(
                   [AC_LANG_PROGRAM(
                     [AC_INCLUDES_DEFAULT(
                       [#include <unistd.h>
                        ])],
                     [getopt (0, NULL, "")])],
                   [AS_VAR_SET(ac_cv_func_getopt, yes)],
                   [AS_VAR_SET(ac_cv_func_getopt, no)])])

AS_IF([test AS_VAR_GET(ac_cv_func_getopt) = yes],
      [AC_DEFINE_UNQUOTED(
        [USE_GETOPT], [1],
        [Define to 1 if you want to use getopt])],
      [])

AC_CACHE_CHECK([for getopt_long], ac_cv_func_getopt_long,
                 [AC_LINK_IFELSE(
                   [AC_LANG_PROGRAM(
                     [AC_INCLUDES_DEFAULT(
                       [#include <unistd.h>
                        ])],
                     [getopt_long (0, NULL, "", NULL, NULL)])],
                   [AS_VAR_SET(ac_cv_func_getopt_long, yes)],
                   [AS_VAR_SET(ac_cv_func_getopt_long, no)])])

AS_IF([test AS_VAR_GET(ac_cv_func_getopt_long) = yes],
      [AC_DEFINE_UNQUOTED(
        [USE_GETOPT_LONG], [1],
        [Define to 1 if you want to use getopt_long])],
      [])


GIFT_DEF_INT_TO_PTR

dnl ###########################################################################
dnl Checks for library functions.
dnl ###########################################################################

AC_FUNC_MEMCMP
AC_FUNC_MMAP
AC_CHECK_FUNCS(ftruncate madvise mkstemp nice select poll signal socket socketpair strdup strstr)
AC_CHECK_FUNCS(snprintf _snprintf vsnprintf _vsnprintf)

dnl ###########################################################################
dnl All of the lovely arguments
dnl ###########################################################################

AC_ARG_ENABLE(daemon,      [  --disable-daemon        disable compiling the daemon],,[enable_daemon=yes])
AM_CONDITIONAL(DAEMON,      test x$enable_daemon   = xyes)
AC_ARG_ENABLE(openft,      [  --disable-openft        disable compiling the OpenFT plugin],,[enable_openft=yes])
AM_CONDITIONAL(OPENFT,      test x$enable_openft   = xyes)
AC_ARG_ENABLE(gnutella,    [  --enable-gnutella       enable compiling the Gnutella plugin],[enable_gnutella=yes],[enable_gnutella=no])
AM_CONDITIONAL(GNUTELLA,    test x$enable_gnutella = xyes)
AC_ARG_ENABLE(perl,        [  --enable-perl           enable perl support],[enable_perl=yes],[enable_perl=no])
AM_CONDITIONAL(PERL,        test x$enable_perl     = xyes)
AC_ARG_ENABLE(ltdl,        [  --disable-ltdl          do not use libltdl],,[enable_ltdl=yes])
AM_CONDITIONAL(LTDL,        test x$enable_ltdl    = xyes)
AC_ARG_ENABLE(id3lib,      [  --disable-id3lib        do not use id3lib],,[use_id3lib=yes])
AM_CONDITIONAL(ID3LIB,      test x$use_id3lib      = xyes)
AC_ARG_ENABLE(libvorbis,   [  --disable-libvorbis     do not use libvorbis],,[use_libvorbis=yes])
AM_CONDITIONAL(LIBVORBIS,   test x$use_libvorbis   = xyes)
AC_ARG_ENABLE(libdb,       [  --enable-libdb          use libdb],[use_libdb=yes],[use_libdb=no])
AM_CONDITIONAL(LIBDB,       test x$use_libdb       = xyes)
AC_ARG_ENABLE(imagemagick, [  --disable-imagemagick   do not use ImageMagick],[use_imagemagick=no],[use_imagemagick=yes])
AM_CONDITIONAL(IMAGEMAGICK, test x$use_imagemagick = xyes)

dnl ###########################################################################
dnl Compile-time plugin support
dnl ###########################################################################

AC_DEFINE(PLUGIN_OPENFT)
AC_SUBST(PLUGIN_OPENFT)

if test x$enable_gnutella = xyes; then
  AC_DEFINE(PLUGIN_GNUTELLA)
  AC_SUBST(PLUGIN_GNUTELLA)
fi

dnl ###########################################################################
dnl Overrides
dnl ###########################################################################

DL_MODULE=
DL_LIBS=
DL_LDFLAGS=
DL_CFLAGS=

# Override -g -O2
CFLAGS="-g -Wall"

dnl ###########################################################################
dnl Architecture specific stuff
dnl ###########################################################################

# AC_CANONICAL_HOST is already REQUIRED by libtool, i think
#AC_CANONICAL_HOST

PROGRAM_EXT=
SO_SUFFIX=so
ZLIB_VER=1.1.4

case "${host}" in
  *-hp-hpux* )
    SO_SUFFIX=sl
  ;;
  *-*-linux* )
    DL_LDFLAGS='-rdynamic'
  ;;
  *-*-cygwin*)
    SO_SUFFIX=dll
    PROGRAM_EXT=".exe"
  ;;

  *-*-darwin* )
    dnl PowerPC & x86 Darwin based distributions (including Mac OS X)
    SO_SUFFIX=dylib
    ZLIB_VER=1.1.3
    DL_CFLAGS="-Ddlsym=dlsym_prepend_underscore"
    DL_LDFLAGS="-dynamic"
  ;;
esac

dnl ###########################################################################
dnl libltdl checks
dnl ###########################################################################

# set the defaults
use_ltdl=no
GIFT_CFLAGS="$CFLAGS"
GIFT_LIBS="$LIBS"

if test x$enable_openft = xyes; then
    GIFT_CFLAGS="$GIFT_CFLAGS -I\$(top_srcdir)/OpenFT -L\$(top_srcdir)/OpenFT"
    GIFT_LIBS="$GIFT_LIBS \$(top_builddir)/OpenFT/libOpenFT.la"
fi

if test x$enable_gnutella = xyes; then
    GIFT_CFLAGS="$GIFT_CFLAGS -I\$(top_srcdir)/Gnutella -L\$(top_srcdir)/Gnutella"
    GIFT_LIBS="$GIFT_LIBS \$(top_builddir)/Gnutella/libGnutella.la"
fi

GIFT_LDFLAGS=""

if test x$enable_ltdl = xyes; then
    dnl check for lt_dlopen and lt_dlsym in libltdl
    AC_CHECK_LIB(ltdl, lt_dlopen,
        [AC_CHECK_LIB(ltdl, lt_dlsym, found_libltdl=yes, found_libltdl=no )] ,)

    dnl check for ltdl header files
    AC_CHECK_HEADERS(ltdl.h,
            [ found_ltdlh=yes ],
            [ found_ltdlh=no ])

    # only add flags for ltdl stuff if all the tests passed
    if (test x$found_libltdl = xyes) && (test x$found_ltdlh = xyes); then
        use_ltdl=yes
        DL_LIBS=-lltdl
        GIFT_LIBS="$LIBS $DL_LIBS"
        GIFT_LDFLAGS="$LDFLAGS $DL_LDFLAGS"
        GIFT_CFLAGS="$CFLAGS $DL_CFLAGS"

        AC_DEFINE(USE_LTDL)
        AC_SUBST(USE_LTDL)
    fi
fi

AC_SUBST(GIFT_CFLAGS)
AC_SUBST(GIFT_LIBS)
AC_SUBST(GIFT_LDFLAGS)

if test x$prefix = xNONE; then
    prefix="/usr/local"
    AC_SUBST(prefix)
fi

plugindir=${prefix}/lib/giFT
AC_SUBST(plugindir)

AC_DEFINE_UNQUOTED(PLUGIN_DIR, "${prefix}/lib/giFT")

giftdatadir=${prefix}/share/giFT
AC_SUBST(giftdatadir)

if test x$WIN32 != xyes; then
    AC_DEFINE_UNQUOTED(DATA_DIR, "${prefix}/share/giFT")
fi

giftperldir=${prefix}/bin
AC_SUBST(giftperldir)

# Automagically include the .conf files into giFT-setup
AC_SUBST_FILE(ui_conf_file)
AC_SUBST_FILE(gift_conf_file)
AC_SUBST_FILE(openft_conf_file)

ui_conf_file=etc/ui/ui.conf
gift_conf_file=etc/gift.conf
openft_conf_file=etc/OpenFT/OpenFT.conf

dnl ###########################################################################
dnl Check for perl/libperl
dnl ###########################################################################

enable_perl=no                         # force off for now
if test x$enable_perl = xyes ; then
    AC_PATH_PROG(sedpath, sed)
    AC_PATH_PROG(perlpath, perl)
    AC_MSG_CHECKING(for Perl compile flags)
    PERL_CFLAGS=`$perlpath -MExtUtils::Embed -e ccopts 2>/dev/null`
    # grrr @ Debian maintainer!!!
    PERL_CFLAGS=`echo $PERL_CFLAGS | $sedpath 's/-D_FILE_OFFSET_BITS=[[^ ]]*//'`
    if test "_$PERL_CFLAGS" = _ ; then
        AC_MSG_RESULT([not found, building without perl.])
        enable_perl=no
    else
        PERL_LIBS=`$perlpath -MExtUtils::Embed -e ldopts 2>/dev/null |$sedpath 's/-lgdbm //'`
        PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-ldb //'`
        PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-lndbm //'`
        if test "$system" = "Linux"; then
            PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-lnsl //'`
            PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-lposix //'`
        fi
        PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-lc //'`
        AC_MSG_RESULT(ok)
        AC_CHECK_LIB(perl,perl_parse,[
        AC_SUBST(PERL_CFLAGS)
        AC_SUBST(PERL_LIBS)
        AC_DEFINE(USE_PERL)
        AC_CHECK_FUNCS(Perl_eval_pv)],enable_perl=no,$PERL_LIBS)
    fi
fi

dnl ###########################################################################
dnl Check for ID3Lib
dnl ###########################################################################

if test x$use_id3lib = xyes
then
  AC_CHECK_LIB(id3, ID3Tag_New,
                [ ID3LIB_CFLAGS=
                  ID3LIB_LIBS=-lid3
                  AC_SUBST(ID3LIB_CFLAGS)
                  AC_SUBST(ID3LIB_LIBS)
                  AC_DEFINE(USE_ID3LIB) ],
               use_id3lib=no)
else
  use_id3lib=no
fi

dnl ###########################################################################
dnl Check for libvorbis
dnl ###########################################################################

if test x$use_libvorbis = xyes
then
  GIFT_CHECK_VORBIS([AC_DEFINE(USE_LIBVORBIS)],
                    [use_libvorbis=no])
else
  use_libvorbis=no
fi

dnl ###########################################################################
dnl Check for libdb{4,3}
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
dnl Check for ImageMagick
dnl ###########################################################################

if test x$use_imagemagick = xyes
then
  OPT_MAGICK="/usr/local"
  AC_ARG_WITH(magick,dnl
    [
--with-Magick[=DIR]       where to look for ImageMagick [compiler/linker default paths]
                          DIR points to the ImageMagick installation prefix [/usr/local]
    ],
    OPT_MAGICK=$withval,
  )

  MAGICK_LDFLAGS="`Magick-config --ldflags`"
  MAGICK_LIBS="`Magick-config --libs`"
  save_LIBS="$LIBS"
  LIBS="$LIBS $MAGICK_LIBS"
  save_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS $MAGICK_LDFLAGS"
  AC_CHECK_LIB(Magick, ReadImage,
                [ IMAGEMAGICK_CFLAGS=
                  IMAGEMAGICK_LDFLAGS="$MAGICK_LDFLAGS"
                  IMAGEMAGICK_LIBS="$MAGICK_LIBS"
                  AC_SUBST(IMAGEMAGICK_CFLAGS)
                  AC_SUBST(IMAGEMAGICK_LDFLAGS)
                  AC_SUBST(IMAGEMAGICK_LIBS)
                  AC_DEFINE(USE_IMAGEMAGICK) ],
               use_imagemagick=no)
  LIBS="$save_LIBS"
  LDFLAGS="$save_LDFLAGS"
else
  use_imagemagick=no
fi

dnl ###########################################################################
dnl Check for the presence of ZLIB libraries and headers
dnl ###########################################################################

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
    AC_CACHE_CHECK([for zlib version ${ZLIB_VER}+ in ${ZLIB_DIR}], zlib_ok,
      AC_TRY_RUN([
        #include <zlib.h>
        #include <string.h>
        void main() {
          exit(strcmp(ZLIB_VERSION, "${ZLIB_VER}") < 0);
        }
      ],
      [
      zlib_ok=yes
      ],
      [
      zlib_ok=no
      ],
      [
      zlib_ok=yes
      ]
      )
    )
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
dnl Finally do the generation
dnl ###########################################################################

AC_CONFIG_FILES([
giFT-setup
gift.spec
Makefile
lib/Makefile
plugin/Makefile
src/Makefile
OpenFT/Makefile
Gnutella/Makefile
data/Makefile
data/OpenFT/Makefile
data/Gnutella/Makefile
])
AC_CONFIG_COMMANDS([default],[[
chmod +x giFT-setup
]],[[]])

AC_OUTPUT

dnl ###########################################################################
dnl Print summary to the user
dnl ###########################################################################

echo
echo "$PACKAGE $VERSION"
echo
echo "-------------------- core ---"
echo
echo "libgift.................: yes"
echo "gift daemon.............: $enable_daemon"
echo "use ltdl................: $use_ltdl"
echo
echo "--------------- protocols ---"
echo
echo "OpenFT..................: $enable_openft"
echo "Gnutella................: $enable_gnutella"
echo
echo "-------------- extensions ---"
echo
echo "use zlib................: $zlib_ok"
echo "use perl................: $enable_perl (support temporarily deprecated)"
echo "use libdb...............: $use_libdb"
echo
echo "--------- meta data tools ---"
echo
echo "use id3lib..............: $use_id3lib"
echo "use libvorbis...........: $use_libvorbis"
echo "use ImageMagick.........: $use_imagemagick"
echo
echo "-----------------------------"
echo
echo "$PACKAGE will be installed in $prefix/bin"
echo
echo "Make sure to get a front-end."
echo "giFTcurs (http://giFTcurs.sourceforge.net) is recommended."
echo
