###############################################################################
# $Id: gift-ltdl.m4,v 1.1 2004/04/17 06:16:28 hipnod Exp $
###############################################################################

AC_DEFUN([GIFT_CHECK_LTDL],
[
    AC_ARG_WITH(ltdl,dnl
    [
  --with-ltdl=PFX             Prefix where libltdl is installed
    ],
    OPT_LTDL=$withval)

    gift_ltdl_save_cflags=$CFLAGS
    gift_ltdl_save_cppflags=$CPPFLAGS
    gift_ltdl_save_libs=$LIBS

    if test x"$OPT_LTDL" != x; then
        DL_CFLAGS="-I$OPT_LTDL/include"
        DL_LDFLAGS="-L$OPT_LTDL/lib"

        CFLAGS="$CFLAGS $DL_CFLAGS"
        CPPFLAGS="$CPPFLAGS $DL_CFLAGS"
        LIBS="$LIBS $DL_LIBS $DL_LDFLAGS"
    fi

    dnl check for lt_dlopen and lt_dlsym in libltdl
    AC_CHECK_LIB(ltdl, lt_dlopen,
        [AC_CHECK_LIB(ltdl, lt_dlsym, found_libltdl=yes, found_libltdl=no )] ,)

    if test x$found_libltdl != xyes; then
      AC_MSG_ERROR([
*** Couldn't find ltdl library.  If it is installed in a non-standard
*** location, please supply --with-ltdl=DIR on the configure command line,
*** where `DIR' is the prefix where ltdl is installed (such as /usr,
*** /usr/local, or /usr/pkg).  If that doesn't work, check config.log.
])
    fi

    dnl check for ltdl header files
    AC_CHECK_HEADERS(ltdl.h,
            [ found_ltdlh=yes ],
            [ found_ltdlh=no ])

    if test x$found_ltdlh != xyes; then
        AC_MSG_ERROR([
*** Couldn't find ltdl.h header file.  The most likely problem is that
*** ltdl.h is not installed.  You may need to install a libltdl-dev or
*** -devel package in order to get ltdl.h.

*** If the file is installed, make sure your compiler can find it is, and
*** that it's installed with the same toplevel prefix as the libltdl
*** library.  You may need to supply a --with-ltdl argument to configure
*** if ltdl.h is in a non-standard location.  Otherwise, check config.log.
])
    fi

    DL_LIBS=-lltdl
    AC_SUBST(DL_LIBS)
    AC_SUBST(DL_LDFLAGS)
    AC_SUBST(DL_CFLAGS)

    AC_DEFINE(USE_LTDL)
    AC_SUBST(USE_LTDL)

    if test x"$OPT_LTDL" != x; then
        CFLAGS=$gift_ltdl_save_cflags
        CPPFLAGS=$gift_ltdl_save_cppflags
        LIBS=$gift_ltdl_save_libs
    fi
])