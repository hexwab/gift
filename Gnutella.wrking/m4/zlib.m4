###############################################################################
## $Id: zlib.m4,v 1.1 2003/09/17 22:26:07 hipnod Exp $
###############################################################################

dnl
dnl Check for zlib in some bizarre locations...
dnl
AC_DEFUN([GIFT_PLUGIN_CHECK_ZLIB], 
[
   # set the minimum ZLIB_VER we're willing to link against...
   ZLIB_VER=1.1.4

   # for some reason Darwin has a 1.1.3 version with the 1.1.4 security fix
   # applied backwards...
   case "${host}" in
   *-*-darwin* )
     ZLIB_VER=1.1.3
   ;;
   esac

   if test x"$OPT_ZLIB" = xno; then
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
])
