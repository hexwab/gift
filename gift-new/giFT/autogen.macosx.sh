#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
PKG_NAME="giFT"
export LDFLAGS="-ldl -L/sw/lib -lltdl -L../OpenFT/.libs"

DIE=0

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: You must have \`autoconf' installed to."
  echo "Use fink to install it:"
  echo "Go to http://fink.sourceforge.net for more information."
  DIE=1
}

(grep "^AM_PROG_LIBTOOL" $srcdir/configure.in >/dev/null) && {
  (glibtool --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have \`glibtool' installed."
    echo "Use fink to install it:"
    echo "Go to http://fink.sourceforge.net for more information."
    DIE=1
  }
}

grep "^AM_GNU_GETTEXT" $srcdir/configure.in >/dev/null && {
  grep "sed.*POTFILES" $srcdir/configure.in >/dev/null || \
  (gettext --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have \`gettext' installed."
    echo "Use fink to install it:"
    echo "Go to http://fink.sourceforge.net for more information."
    DIE=1
  }
}

grep "^AM_GNOME_GETTEXT" $srcdir/configure.in >/dev/null && {
  grep "sed.*POTFILES" $srcdir/configure.in >/dev/null || \
  (gettext --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have \`gettext' installed."
    echo "Use fink to install it:"
    echo "Go to http://fink.sourceforge.net for more information."
    DIE=1
  }
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: You must have \`automake' installed."
    echo "Use fink to install it:"
    echo "Go to http://fink.sourceforge.net for more information."
  DIE=1
  NO_AUTOMAKE=yes
}


# if no automake, don't bother testing for aclocal
test -n "$NO_AUTOMAKE" || (aclocal --version) < /dev/null > /dev/null
2>&1 || {
  echo
  echo "**Error**: Missing \`aclocal'.  The version of \`automake'"
  echo "installed doesn't appear recent enough."
  echo "Use fink to install it:"
  echo "Go to http://fink.sourceforge.net for more information."
  DIE=1
}

if test "$DIE" -eq 1; then
  exit 1
fi

if test -z "$*"; then
  echo "I am going to run \`configure' with the default arguments."
  echo "If you wish to pass any other to it, please specify them on
  the"
  echo \`$0\'" command line."
  echo
  1="ppc"
  2="--disable-libdl"
  3="--with-zlib=\"/usr/include/net/\""
  echo "Default is:"
  echo "./configure $*"
  echo
else
  echo "**Warning**: To compile giFT on Mac OS X you must specify"
  echo "\"ppc\" as the host and "--disable-libdl" as parameters"
  echo "for configure. Otherwise giFT will not compile."
  echo
  echo "Parameters are:"
  echo "./configure $*"
fi

case $CC in
xlc )
  am_opt=--include-deps;;
esac

for coin in `find $srcdir -name configure.in -print`
do
  dr=`dirname $coin`
  if test -f $dr/NO-AUTO-GEN; then
    echo skipping $dr -- flagged as no auto-gen
  else
    echo processing $dr
    macrodirs=`sed -n -e 's,AM_ACLOCAL_INCLUDE(\(.*\)),\1,gp' < $coin`
    ( cd $dr
      aclocalinclude="$ACLOCAL_FLAGS"
      for k in $macrodirs; do
        if test -d $k; then
          aclocalinclude="$aclocalinclude -I $k"
        ##else
        ##  echo "**Warning**: No such directory \`$k'.  Ignored."
        fi
      done
      if grep "^AM_GNU_GETTEXT" configure.in >/dev/null; then
        if grep "sed.*POTFILES" configure.in >/dev/null; then
          : do nothing -- we still have an old unmodified configure.in
        else
          echo "Creating $dr/aclocal.m4 ..."
          test -r $dr/aclocal.m4 || touch $dr/aclocal.m4
          echo "Running gettextize...  Ignore non-fatal messages."
          echo "no" | gettextize --force --copy
          echo "Making $dr/aclocal.m4 writable ..."
          test -r $dr/aclocal.m4 && chmod u+w $dr/aclocal.m4
        fi
      fi
      if grep "^AM_GNOME_GETTEXT" configure.in >/dev/null; then
        echo "Creating $dr/aclocal.m4 ..."
        test -r $dr/aclocal.m4 || touch $dr/aclocal.m4
        echo "Running gettextize...  Ignore non-fatal messages."
        echo "no" | gettextize --force --copy
        echo "Making $dr/aclocal.m4 writable ..."
        test -r $dr/aclocal.m4 && chmod u+w $dr/aclocal.m4
      fi
      if grep "^AM_PROG_LIBTOOL" configure.in >/dev/null; then
        echo "Running glibtoolize..."
        glibtoolize --force --copy
      fi
      echo "Running aclocal $aclocalinclude ..."
      aclocal $aclocalinclude
      if grep "^AM_CONFIG_HEADER" configure.in >/dev/null; then
        echo "Running autoheader..."
        autoheader
      fi
      echo "Running automake --gnu $am_opt ..."
      automake --add-missing --gnu $am_opt
      echo "Running autoconf ..."
      autoconf
    )
  fi
done

#conf_flags="--enable-maintainer-mode --enable-compile-warnings"
#--enable-iso-c

if test x$NOCONFIGURE = x; then
  echo Running $srcdir/configure $conf_flags "$@" ...
  $srcdir/configure $conf_flags "$@" \
  && echo Now type \`make\' to compile $PKG_NAME
else
  echo Skipping configure process.
fi
