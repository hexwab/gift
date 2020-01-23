dnl Curses detection: Munged from Midnight Commander's configure.in
dnl Modifications for giFTcurs:
dnl . Added a check for OS X specific paths.
dnl . Somewhat modified to check for use_default_colors and resizeterm.
dnl . Trimmed away the BSD check.
dnl . Updated AC_DEFINE macros to not need acconfig.h, and therefore also
dnl   trimmed the explanation below.
dnl . Removed SCO references.
dnl . Added a --with-ncursesw flag.
dnl . Added the has_wide_curses variable.
dnl . Removed ncurses version checking.
dnl
dnl What it does:
dnl =============
dnl
dnl - Determine which version of curses is installed on your system
dnl   and set the -I/-L/-l compiler entries and add a few preprocessor
dnl   symbols 
dnl - Do an AC_SUBST on the CURSES_INCLUDEDIR and CURSES_LIBS so that
dnl   @CURSES_INCLUDEDIR@ and @CURSES_LIBS@ will be available in
dnl   Makefile.in's
dnl - Modify the following configure variables (these are the only
dnl   curses.m4 variables you can access from within configure.in)
dnl   CURSES_INCLUDEDIR - contains -I's and possibly -DRENAMED_CURSES if
dnl                       an ncurses.h that's been renamed to curses.h
dnl                       is found.
dnl   CURSES_LIBS       - sets -L and -l's appropriately
dnl   CFLAGS            - if --with-sco, add -D_SVID3 
dnl   has_curses        - exports result of tests to rest of configure
dnl
dnl Usage:
dnl ======
dnl 1) call AC_CHECK_CURSES after AC_PROG_CC in your configure.in
dnl 2) Instead of #include <curses.h> you should use the following to
dnl    properly locate ncurses or curses header file
dnl
dnl    #if defined(USE_NCURSES) && !defined(RENAMED_NCURSES)
dnl    #include <ncurses.h>
dnl    #else
dnl    #include <curses.h>
dnl    #endif
dnl
dnl 3) Make sure to add @CURSES_INCLUDEDIR@ to your preprocessor flags
dnl 4) Make sure to add @CURSES_LIBS@ to your linker flags or LIBS
dnl
dnl Notes with automake:
dnl - call AM_CONDITIONAL(HAS_CURSES, test "$has_curses" = true) from
dnl   configure.in
dnl - your Makefile.am can look something like this
dnl   -----------------------------------------------
dnl   INCLUDES= blah blah blah $(CURSES_INCLUDEDIR) 
dnl   if HAS_CURSES
dnl   CURSES_TARGETS=name_of_curses_prog
dnl   endif
dnl   bin_PROGRAMS = other_programs $(CURSES_TARGETS)
dnl   other_programs_SOURCES = blah blah blah
dnl   name_of_curses_prog_SOURCES = blah blah blah
dnl   other_programs_LDADD = blah
dnl   name_of_curses_prog_LDADD = blah $(CURSES_LIBS)
dnl   -----------------------------------------------


AC_DEFUN([AC_CHECK_CURSES],[
	search_ncurses=true
	screen_manager=""
	has_curses=false
	has_wide_curses=no

	CFLAGS=${CFLAGS--O}

	AC_SUBST(CURSES_LIBS)
	AC_SUBST(CURSES_INCLUDEDIR)

	AC_ARG_WITH(sunos-curses,
	  [  --with-sunos-curses     used to force SunOS 4.x curses],[
	  if test x$withval = xyes; then
		AC_USE_SUNOS_CURSES
	  fi
	])

	AC_ARG_WITH(osf1-curses,
	  [  --with-osf1-curses      used to force OSF/1 curses],[
	  if test x$withval = xyes; then
		AC_USE_OSF1_CURSES
	  fi
	])

	AC_ARG_WITH(vcurses,
	  [[  --with-vcurses[=incdir] used to force SysV curses]],
	  if test x$withval != xyes; then
		CURSES_INCLUDEDIR="-I$withval"
	  fi
	  AC_USE_SYSV_CURSES
	)

	AC_ARG_WITH(ncurses,
	  [[  --with-ncurses[=dir]    compile with ncurses/locate base dir]],
	  if test x$withval = xno ; then
		search_ncurses=false
	  elif test x$withval != xyes ; then
		CURSES_LIBS="$LIBS -L$withval/lib -lncurses"
		CURSES_INCLUDEDIR="-I$withval/include"
		if test -f $withval/include/curses.h
		then
			CURSES_INCLUDEDIR="$CURSES_INCLUDEDIR -DRENAMED_NCURSES"
		fi
		search_ncurses=false
		screen_manager="ncurses"
		AC_DEFINE(USE_NCURSES, 1, [Use Ncurses?])
		AC_DEFINE(HAS_CURSES, 1, [Found some version of curses that we're going to use])
		has_curses=true
	  fi
	)

	AC_ARG_WITH(ncursesw,
	  [[  --with-ncursesw[=dir]   compile with ncursesw/locate base dir]],
	  if test x$withval = xyes; then
		AC_NCURSES(/usr/include/ncursesw, curses.h, -lncursesw, -I/usr/include/ncursesw -DRENAMED_NCURSES, renamed ncursesw on /usr/include/ncursesw)
	    search_ncurses=false
	  elif test x$withval != xyes ; then
		CURSES_LIBS="$LIBS -L$withval/lib -lncursesw"
		CURSES_INCLUDEDIR="-I$withval/include"
		if test -f $withval/include/curses.h
		then
			CURSES_INCLUDEDIR="$CURSES_INCLUDEDIR -DRENAMED_NCURSES"
		fi
	    search_ncurses=false
		screen_manager="ncursesw"
		AC_DEFINE(USE_NCURSES, 1)
		AC_DEFINE(HAS_CURSES, 1)
		has_curses=true
	  fi
	)

	if $search_ncurses
	then
		AC_SEARCH_NCURSES()
	fi

	dnl Check for some functions
	SAVED_LIBS="$LIBS"
	LIBS="$CURSES_LIBS"
	unset ac_cv_func_wadd_wch
	AC_CHECK_FUNCS(use_default_colors resizeterm resize_term wadd_wch)
	LIBS="$SAVED_LIBS"

	dnl See if it's a wide curses
	if test $ac_cv_func_wadd_wch = yes; then
		has_wide_curses=yes
		AH_VERBATIM([_XOPEN_SOURCE_EXTENDED],
		[/* Enable X/Open Unix extensions */
#ifndef _XOPEN_SOURCE_EXTENDED
# define _XOPEN_SOURCE_EXTENDED
#endif])
		AC_DEFINE(WIDE_NCURSES, 1, [curses routines to work with wide chars are available])
	fi
])


AC_DEFUN([AC_USE_SUNOS_CURSES], [
	search_ncurses=false
	screen_manager="SunOS 4.x /usr/5include curses"
	AC_MSG_RESULT(Using SunOS 4.x /usr/5include curses)
	AC_DEFINE(USE_SUNOS_CURSES, 1, [Use SunOS SysV curses?])
	AC_DEFINE(HAS_CURSES, 1)
	has_curses=true
	AC_DEFINE(NO_COLOR_CURSES, 1, [If your curses does not have color define this one])
	AC_DEFINE(USE_SYSV_CURSES, 1, [Use SystemV curses?])
	CURSES_INCLUDEDIR="-I/usr/5include"
	CURSES_LIBS="/usr/5lib/libcurses.a /usr/5lib/libtermcap.a"
	AC_MSG_RESULT(Please note that some screen refreshes may fail)
])

AC_DEFUN([AC_USE_OSF1_CURSES], [
       AC_MSG_RESULT(Using OSF1 curses)
       search_ncurses=false
       screen_manager="OSF1 curses"
       AC_DEFINE(HAS_CURSES, 1)
       has_curses=true
       AC_DEFINE(NO_COLOR_CURSES, 1)
       AC_DEFINE(USE_SYSV_CURSES, 1)
       CURSES_LIBS="-lcurses"
])

AC_DEFUN([AC_USE_SYSV_CURSES], [
	AC_MSG_RESULT(Using SysV curses)
	AC_DEFINE(HAS_CURSES, 1)
	has_curses=true
	AC_DEFINE(USE_SYSV_CURSES)
	search_ncurses=false
	screen_manager="SysV/curses"
	CURSES_LIBS="-lcurses"
])

dnl
dnl Parameters: directory filename curses_LIBS curses_INCLUDEDIR nicename
dnl
AC_DEFUN([AC_NCURSES], [
    if $search_ncurses
    then
        if test -f $1/$2
	then
	    AC_MSG_RESULT(Found ncurses on $1/$2)
 	    CURSES_LIBS="$3"
	    CURSES_INCLUDEDIR="$4"
	    search_ncurses=false
	    screen_manager="$5"
            AC_DEFINE(HAS_CURSES, 1)
            has_curses=true
	    AC_DEFINE(USE_NCURSES, 1)
	fi
    fi
])

AC_DEFUN([AC_SEARCH_NCURSES], [
    AC_CHECKING(location of ncurses.h file)

    AC_NCURSES(/usr/include, ncurses.h, -lncurses,, ncurses on /usr/include)
    AC_NCURSES(/usr/include/ncurses, ncurses.h, -lncurses, -I/usr/include/ncurses, ncurses on /usr/include/ncurses)
    AC_NCURSES(/usr/local/include, ncurses.h, -L/usr/local/lib -lncurses, -I/usr/local/include, ncurses on /usr/local)
    AC_NCURSES(/usr/local/include/ncurses, ncurses.h, -L/usr/local/lib -L/usr/local/lib/ncurses -lncurses, -I/usr/local/include/ncurses, ncurses on /usr/local/include/ncurses)

    dnl ncurses hides here on OS X
    AC_NCURSES(/sw/include, ncurses.h, -L/sw/lib -lncurses, -I/sw/include, ncurses on /sw/include)

    AC_NCURSES(/usr/local/include/ncurses, curses.h, -L/usr/local/lib -lncurses, -I/usr/local/include/ncurses -DRENAMED_NCURSES, renamed ncurses on /usr/local/.../ncurses)

    AC_NCURSES(/usr/include/ncurses, curses.h, -lncurses, -I/usr/include/ncurses -DRENAMED_NCURSES, renamed ncurses on /usr/include/ncurses)

    dnl
    dnl We couldn't find ncurses, try SysV curses
    dnl
    if $search_ncurses 
    then
        AC_EGREP_HEADER(init_color, /usr/include/curses.h,
	    AC_USE_SYSV_CURSES)
	AC_EGREP_CPP(USE_NCURSES,[
#include <curses.h>
#ifdef __NCURSES_H
#undef USE_NCURSES
USE_NCURSES
#endif
],[
	CURSES_INCLUDEDIR="$CURSES_INCLUDEDIR -DRENAMED_NCURSES"
        AC_DEFINE(HAS_CURSES, 1)
	has_curses=true
        AC_DEFINE(USE_NCURSES, 1)
        search_ncurses=false
        screen_manager="ncurses installed as curses"
])
    fi

    dnl
    dnl Try SunOS 4.x /usr/5{lib,include} ncurses
    dnl The flags USE_SUNOS_CURSES, USE_BSD_CURSES and BUGGY_CURSES
    dnl should be replaced by a more fine grained selection routine
    dnl
	if $search_ncurses; then
		if test -f /usr/5include/curses.h
		then
			AC_USE_SUNOS_CURSES
		fi
	fi
])

dnl Check if gcc accepts the -no-cpp-precomp flag. (Mac OS X thingee)
dnl AC_NO_CPP_PRECOMP
AC_DEFUN(AC_NO_CPP_PRECOMP,
[
	AC_CACHE_CHECK([if $CC needs -no-cpp-precomp],
				   [ac_no_cpp_precomp],
				   [echo "void f(){}" > conftest.c
					if test -z "`${CC} -no-cpp-precomp -c conftest.c 2>&1`"; then
						ac_no_cpp_precomp=yes
					else
						ac_no_cpp_precomp=no
					fi
					rm -f conftest*
				   ])
	if test "x$ac_no_cpp_precomp" = "xyes"; then
		CFLAGS="$CFLAGS -no-cpp-precomp"
	fi
])

dnl Check what flags gcc accepts and add them to CFLAGS
dnl AX_TRY_GCC_FLAGS([flags])
dnl Written by Go"ran Weinholt.
AC_DEFUN(AX_TRY_GCC_FLAGS,
[
	if test "x$GCC" = xyes; then
		AC_CACHE_CHECK([for flags to pass to gcc], ax_cv_try_gcc_flags,
					   [echo "void f(void){}" > conftest.c
						ax_cv_try_gcc_flags=
						for flag in $1; do
							if test -z "`${CC} $flag -c conftest.c 2>&1`"; then
								ax_cv_try_gcc_flags="$flag $ax_cv_try_gcc_flags"
							fi
						done
						rm -f conftest.c conftest.$ac_ext
					   ])
		if test -n "$ax_cv_try_gcc_flags"; then
			CFLAGS="$CFLAGS $ax_cv_try_gcc_flags"
		fi
	fi
])

# Configure paths for GLIB
# Owen Taylor     1997-2001

dnl AM_PATH_GLIB_2_0([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, MODULES]]]])
dnl Test for GLIB, and define GLIB_CFLAGS and GLIB_LIBS, if gmodule, gobject or 
dnl gthread is specified in MODULES, pass to pkg-config
dnl
AC_DEFUN(AM_PATH_GLIB_2_0,
[dnl 
dnl Get the cflags and libraries from pkg-config
dnl
AC_ARG_ENABLE(glibtest, [  --disable-glibtest      do not try to compile and run a test GLIB program],
		    , enable_glibtest=yes)

  pkg_config_args=glib-2.0
  for module in . $4
  do
      case "$module" in
         gmodule) 
             pkg_config_args="$pkg_config_args gmodule-2.0"
         ;;
         gobject) 
             pkg_config_args="$pkg_config_args gobject-2.0"
         ;;
         gthread) 
             pkg_config_args="$pkg_config_args gthread-2.0"
         ;;
      esac
  done

  AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

  no_glib=""

  if test x$PKG_CONFIG != xno ; then
    if $PKG_CONFIG --atleast-pkgconfig-version 0.7 ; then
      :
    else
      echo *** pkg-config too old; version 0.7 or better required.
      no_glib=yes
      PKG_CONFIG=no
    fi
  else
    no_glib=yes
  fi

  min_glib_version=ifelse([$1], ,2.0.0,$1)
  AC_MSG_CHECKING(for GLIB - version >= $min_glib_version)

  if test x$PKG_CONFIG != xno ; then
    ## don't try to run the test against uninstalled libtool libs
    if $PKG_CONFIG --uninstalled $pkg_config_args; then
	  echo "Will use uninstalled version of GLib found in PKG_CONFIG_PATH"
	  enable_glibtest=no
    fi

    if $PKG_CONFIG --atleast-version $min_glib_version $pkg_config_args; then
	  :
    else
	  no_glib=yes
    fi
  fi

  if test x"$no_glib" = x ; then
    GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
    GOBJECT_QUERY=`$PKG_CONFIG --variable=gobject_query glib-2.0`
    GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`

    GLIB_CFLAGS=`$PKG_CONFIG --cflags $pkg_config_args`
    GLIB_LIBS=`$PKG_CONFIG --libs $pkg_config_args`
    glib_config_major_version=`$PKG_CONFIG --modversion glib-2.0 | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    glib_config_minor_version=`$PKG_CONFIG --modversion glib-2.0 | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    glib_config_micro_version=`$PKG_CONFIG --modversion glib-2.0 | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_glibtest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GLIB_CFLAGS"
      LIBS="$GLIB_LIBS $LIBS"
dnl
dnl Now check if the installed GLIB is sufficiently new. (Also sanity
dnl checks the results of pkg-config to some extent)
dnl
      rm -f conf.glibtest
      AC_TRY_RUN([
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>

int 
main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.glibtest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = g_strdup("$min_glib_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_glib_version");
     exit(1);
   }

  if ((glib_major_version != $glib_config_major_version) ||
      (glib_minor_version != $glib_config_minor_version) ||
      (glib_micro_version != $glib_config_micro_version))
    {
      printf("\n*** 'pkg-config --modversion glib-2.0' returned %d.%d.%d, but GLIB (%d.%d.%d)\n", 
             $glib_config_major_version, $glib_config_minor_version, $glib_config_micro_version,
             glib_major_version, glib_minor_version, glib_micro_version);
      printf ("*** was found! If pkg-config was correct, then it is best\n");
      printf ("*** to remove the old version of GLib. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If pkg-config was wrong, set the environment variable PKG_CONFIG_PATH\n");
      printf("*** to point to the correct configuration files\n");
    } 
  else if ((glib_major_version != GLIB_MAJOR_VERSION) ||
	   (glib_minor_version != GLIB_MINOR_VERSION) ||
           (glib_micro_version != GLIB_MICRO_VERSION))
    {
      printf("*** GLIB header files (version %d.%d.%d) do not match\n",
	     GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
      printf("*** library (version %d.%d.%d)\n",
	     glib_major_version, glib_minor_version, glib_micro_version);
    }
  else
    {
      if ((glib_major_version > major) ||
        ((glib_major_version == major) && (glib_minor_version > minor)) ||
        ((glib_major_version == major) && (glib_minor_version == minor) && (glib_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of GLIB (%d.%d.%d) was found.\n",
               glib_major_version, glib_minor_version, glib_micro_version);
        printf("*** You need a version of GLIB newer than %d.%d.%d. The latest version of\n",
	       major, minor, micro);
        printf("*** GLIB is always available from ftp://ftp.gtk.org.\n");
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the pkg-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of GLIB, but you can also set the PKG_CONFIG environment to point to the\n");
        printf("*** correct copy of pkg-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
    }
  return 1;
}
],, no_glib=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_glib" = x ; then
     AC_MSG_RESULT(yes (version $glib_config_major_version.$glib_config_minor_version.$glib_config_micro_version))
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$PKG_CONFIG" = "no" ; then
       echo "*** A new enough version of pkg-config was not found."
       echo "*** See http://www.freedesktop.org/software/pkgconfig/"
     else
       if test -f conf.glibtest ; then
        :
       else
          echo "*** Could not run GLIB test program, checking why..."
          ac_save_CFLAGS="$CFLAGS"
          ac_save_LIBS="$LIBS"
          CFLAGS="$CFLAGS $GLIB_CFLAGS"
          LIBS="$LIBS $GLIB_LIBS"
          AC_TRY_LINK([
#include <glib.h>
#include <stdio.h>
],      [ return ((glib_major_version) || (glib_minor_version) || (glib_micro_version)); ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GLIB or finding the wrong"
          echo "*** version of GLIB. If it is not finding GLIB, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH" ],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GLIB is incorrectly installed."])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     GLIB_CFLAGS=""
     GLIB_LIBS=""
     GLIB_GENMARSHAL=""
     GOBJECT_QUERY=""
     GLIB_MKENUMS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GLIB_CFLAGS)
  AC_SUBST(GLIB_LIBS)
  AC_SUBST(GLIB_GENMARSHAL)
  AC_SUBST(GOBJECT_QUERY)
  AC_SUBST(GLIB_MKENUMS)
  rm -f conf.glibtest
])
