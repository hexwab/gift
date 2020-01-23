# -*- sh -*-

# Process this file with autoconf to produce a configure script.
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



# Checks for programs.
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PROG_INSTALL

AM_DISABLE_STATIC
AM_PROG_LIBTOOL

# Checks for libraries.
AC_CHECK_LIB(nsl, gethostbyname)
AC_CHECK_LIB(socket, socket)

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(dirent.h fcntl.h limits.h linux/limits.h sys/mman.h sys/time.h syslog.h unistd.h)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

AC_C_BIGENDIAN

AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(void *)
GIFT_DEF_INT_TO_PTR

# Checks for library functions.
AC_FUNC_MEMCMP
AC_FUNC_MMAP
AC_CHECK_FUNCS(madvise nice select socket strdup strstr)
# not used:
# AC_TRY_COMPILE([#include <sys/types.h>
# #include <sys/socket.h>], [socklen_t slen;],[],[AC_DEFINE(NEED_SOCKLEN_T)])

# All of the lovely arguments
AC_ARG_ENABLE(daemon,[  --disable-daemon        disable compiling the daemon],,[enable_daemon=yes])
AM_CONDITIONAL(DAEMON, test x$enable_daemon = xyes)
AC_ARG_ENABLE(openft,[  --disable-openft        disable compiling the OpenFT plugin],,[enable_openft=yes])
AM_CONDITIONAL(OPENFT, test x$enable_openft = xyes)
AC_ARG_ENABLE(perl,[  --disable-perl          disable perl support],,[enable_perl=yes])
AM_CONDITIONAL(PERL, test x$enable_perl = xyes)
AC_ARG_ENABLE(gtk-client,[  --disable-gtk-client    disable compiling the GTK+ frontend],,[enable_gtk_client=no])
AM_CONDITIONAL(GTK_CLIENT, test x$enable_gtk_client = xyes)
AC_ARG_ENABLE(libdl,[  --disable-libdl         do not use libdl],,[enable_libdl=yes])
AM_CONDITIONAL(LIBDL, test x$enable_libdl = xyes)

DL_MODULE=
DL_LIBS=
DL_LDFLAGS=

# Architecture specific stuff
AC_CANONICAL_HOST

PROGRAM_EXT=
SO_SUFFIX=so

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

  powerpc-*-darwin* )
    dnl PowerPC Darwin based distributions (including Mac OS X)
    SO_SUFFIX=dylib
  ;;
esac

# libdl checks
if test x$enable_libdl = xyes; then
	AC_DEFINE(USE_DLOPEN)
	AC_SUBST(USE_DLOPEN)

	dnl check for dlopen and dlsym in the native c library
	AC_CHECK_FUNC(dlopen,
		AC_CHECK_FUNC(dlsym,
			DL_MODULE=DL_MODULE_DL
		,)
	,)

	dnl check for dlopen and dlsym in libdl
	AC_CHECK_LIB(dl, dlopen,
		AC_CHECK_LIB(dl, dlsym,
			DL_LIBS=-ldl
			DL_MODULE=DL_MODULE_DL
		,)
		,)

	dnl TODO - HPUX

	GIFT_LIBS="$LIBS $DL_LIBS"
	GIFT_LDFLAGS="$LDFLAGS $DL_LDFLAGS"
else
	GIFT_CFLAGS="-L\$(top_srcdir)/OpenFT"
	GIFT_LIBS="$LIBS \$(top_builddir)/OpenFT/libOpenFT.la"
	GIFT_LDFLAGS=""
fi

# Override -g -O2
CFLAGS="-g -Wall"

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

giftconfdir=${prefix}/etc/giFT
AC_SUBST(giftconfdir)

dnl Check for perl/libperl

if test "$enable_perl" = yes ; then
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

dnl Check for the presence of ZLIB libraries and headers
ZLIB_VER=1.1.4

dnl Default to compiler & linker defaults for files & libraries.
OPT_ZLIB=
AC_ARG_WITH(zlib,dnl
[  --with-zlib[=DIR]         where to look for ZLIB [compiler/linker default paths]
                          DIR points to the ZLIB installation prefix [/usr/local]],
 OPT_ZLIB=$withval,
)

AC_MSG_CHECKING(where to look for ZLIB)
if test X"$OPT_ZLIB" = Xno
then
  AC_MSG_RESULT([no zlib library])
else
  dnl z lib?
  AC_CHECK_FUNC(gzread, , AC_CHECK_LIB(z, gzread))

  ZLIB_DIRS="$OPT_ZLIB /usr /usr/local"
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
      ]
      )
    )
    AC_MSG_RESULT([$zlib_ok])
    if test "$zlib_ok" = "yes"; then
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

If zlib ${ZLIB_VER}+ is not installed, use the following commands to install it:

$ make -f zlib.mak
$ su -c 'make -f zlib.mak install'

zlib.mak installs zlib in the /usr/local directory.

Next, run ./autogen.sh and specify the directory where zlib is installed. For example:

$ ./autogen.sh --with-zlib=/usr/local

    ])
  fi
fi

# Checks for GTK+
if test x$enable_gtk_client = xyes ; then
#	AM_PATH_GTK(,,[AM_CONDITIONAL(GTK_CLIENT, false); enable_gtk_client=no])
	AM_PATH_GTK(1.2.0,,AC_MSG_ERROR([*** GTK+ >= 1.2.0 not fully installed - please either install development packages or build with --disable-gtk-client]))
	GTKCFLAGS="$GTK_CFLAGS"
	GTKLIBS="$GTK_LIBS"
	AC_SUBST(GTKCFLAGS)
	AC_SUBST(GTKLIBS)
fi

# lifted almost verbatim from glib1.3's configure.in

AC_OUTPUT_COMMANDS([

## Generate `giftconfig.h' in two cases
## 1. `config.status' is run either explicitly, or via configure.
##     Esp. not when it is run in `Makefile' to generate makefiles and
##     config.h
## 2. CONFIG_OTHER is set explicitly
##
## Case 1 is difficult.  We know that `automake' sets one of
## CONFIG_FILES or CONFIG_HEADERS to empty.  This heuristic works
## only when AM_CONFIG_HEADER is set, however.

if test -n "${CONFIG_FILES}" && test -n "${CONFIG_HEADERS}"; then
  # Both CONFIG_FILES and CONFIG_HEADERS are non-empty ==> Case 1
  CONFIG_OTHER=${CONFIG_OTHER:-giftconfig.h}
fi
case "$CONFIG_OTHER" in
*giftconfig.h*)
	echo creating giftconfig.h
	outfile=giftconfig.h-tmp
	cat > $outfile <<\_______EOF
/* giftconfig.h
 *
 * This is a generated file.  Please modify 'configure.in'
 */

#ifndef __GIFTCONFIG_H
#define __GIFTCONFIG_H

_______EOF

	### this should always be true in a modern C/C++ compiler
	cat >>$outfile <<_______EOF
typedef signed char ft_int8;
typedef unsigned char ft_uint8;
_______EOF


	if test -n "$ft_int16"; then
	  cat >>$outfile <<_______EOF
typedef signed $ft_int16 ft_int16;
typedef unsigned $ft_int16 ft_uint16;
#define G_FT_INT16_FORMAT $ft_int16_format
#define G_FT_UINT16_FORMAT $ft_uint16_format
_______EOF
	fi


	if test -n "$ft_int32"; then
	  cat >>$outfile <<_______EOF
typedef signed $ft_int32 ft_int32;
typedef unsigned $ft_int32 ft_uint32;
#define G_FT_INT32_FORMAT $ft_int32_format
#define G_FT_UINT32_FORMAT $ft_uint32_format
_______EOF
	fi

	cat >>$outfile <<_______EOF
	
#define I_PTR(x)      ((void *)($voidptr)x)
#define P_INT(x)      (($voidptr)x)

#endif /* __GIFTCONFIG_H */
_______EOF

	if cmp -s $outfile giftconfig.h; then
	  echo giftconfig.h is unchanged
	  rm -f $outfile
	else
	  mv $outfile giftconfig.h
	fi ;;
esac
],[

# Note that if two cases are the same, case goes with the first one.
# Note also that this is inside an AC_OUTPUT_COMMAND.  We do not depend
# on variable expansion in case labels.  Look at the generated config.status
# for a hint.

case 2 in
$ac_cv_sizeof_short)
  ft_int16=short
  ft_int16_format='"hi"'
  ft_uint16_format='"hu"'
  ;;
$ac_cv_sizeof_int)
  ft_int16=int
  ft_int16_format='"i"'
  ft_uint16_format='"u"'
  ;;
esac
case 4 in
$ac_cv_sizeof_short)
  ft_int32=short
  ft_int32_format='"hi"'
  ft_uint32_format='"hu"'
  ;;
$ac_cv_sizeof_int)
  ft_int32=int
  ft_int32_format='"i"'
  ft_uint32_format='"u"'
  ;;
$ac_cv_sizeof_long)
  ft_int32=long
  ft_int32_format='"li"'
  ft_uint32_format='"lu"'
  ;;
esac
case 8 in
$ac_cv_sizeof_int)
  ft_int64=int
  ft_int64_format='"i"'
  ft_uint64_format='"u"'
  gift_extension=
  ft_int64_constant='(val)'
  ;;
$ac_cv_sizeof_long)
  ft_int64=long
  ft_int64_format='"li"'
  ft_uint64_format='"lu"'
  gift_extension=
  ft_int64_constant='(val##L)'
  ;;
esac
gift_size_t=$gift_cv_sizeof_size_t
gift_void_p=$ac_cv_sizeof_long
gift_long=$ac_cv_sizeof_void_p

gintbits=`expr $ac_cv_sizeof_int \* 8`
glongbits=`expr $ac_cv_sizeof_long \* 8`

case $ac_cv_sizeof_void_p in
$ac_cv_sizeof_int)  	gift_gpi_cast=''        gift_gpui_cast=''         ;;
$ac_cv_sizeof_long) 	gift_gpi_cast='(glong)' gift_gpui_cast='(gulong)' ;;
*) 			gift_unknown_void_p=yes ;;
esac
])

# god autoconf is a damn nightmare
AC_CONFIG_FILES([
giFT-setup
gift.spec
Makefile
lib/Makefile
src/Makefile
ui/Makefile
OpenFT/Makefile
ui/perl/Makefile
data/Makefile
data/OpenFT/Makefile
etc/Makefile
etc/OpenFT/Makefile
etc/ui/Makefile
])
AC_CONFIG_COMMANDS([default],[[
chmod +x giFT-setup
]],[[]])

AC_OUTPUT

echo
echo $PACKAGE $VERSION
echo
echo Build Daemon............: $enable_daemon
echo Build OpenFT............: $enable_openft
echo Build GTK+ Frontend.....: $enable_gtk_client \(deprecated, see giftcurs.sf.net\)
echo Use dlopen..............: $enable_libdl
echo Use zlib................: $zlib_ok
echo Use perl................: $enable_perl
echo
echo $PACKAGE will be installed in $prefix/bin
echo
echo configure complete, now type \'make\'
echo
