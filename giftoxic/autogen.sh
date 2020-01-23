#! /bin/sh
# Run this to generate all the initial makefiles, etc.

# Stolen from giFTcurs. Modified for giFToxic

# Make it possible to specify path in the environment
: ${AUTOCONF=autoconf}
: ${AUTOHEADER=autoheader}
: ${AUTOMAKE=automake}
: ${ACLOCAL=aclocal}
: ${GETTEXTIZE=gettextize}

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

(
cd $srcdir

# The autoconf cache (version after 2.52) is not reliable yet.
rm -rf autom4te.cache

# Ensure that gettext is reasonably new.
gettext_ver=`$GETTEXTIZE --version | sed -n '1s/\.//g;1s/.* //;1s/^\(...\)$/\100/;1s/^\(...\)\(.\)$/\10\2/;1p'`
if test $gettext_ver -lt 01038; then
  echo "Don't use gettext older than 0.10.38" 2>&1
  exit 1
fi

echo "Running $GETTEXTIZE..."
rm -rf intl
if test $gettext_ver -ge 01100; then
  if test $gettext_ver -ge 01104; then
    # gettextize doesn't want us to run it in a non-interactive script.
    GETTEXTIZE_FIXED=./gettextize.fixed
    egrep -v "read.*/dev/tty" `which $GETTEXTIZE` > $GETTEXTIZE_FIXED
	chmod +x $GETTEXTIZE_FIXED
	GETTEXTIZE=$GETTEXTIZE_FIXED
  fi

  $GETTEXTIZE --copy --force --intl --no-changelog >tmpout || exit 1

  if test -f Makefile.am~; then
     rm -rf Makefile.am
     mv Makefile.am~ Makefile.am
  fi
  if test -f configure.in~ ; then
    rm -rf configure.in
    mv configure.in~ configure.in
  fi
  for i in po/Rules-quot po/boldquot.sed po/en@boldquot.header \
	   po/en@quot.header po/insert-header.sin po/quot.sed \
	   po/remove-potcdate.sin po/Makefile.in.in
  do
    if diff -s $i $i~ >/dev/null 2>&1 ; then
      mv $i~ $i
    fi
  done
  # Does we need po/Makevars.in and family for gettext 0.11 ?
  if test ! -f po/Makevars; then
    cat > po/Makevars <<\EOF
# Usually the message domain is the same as the package name.
DOMAIN = $(PACKAGE)

# These two variables depend on the location of this directory.
subdir = po
top_builddir = ..

# These options get passed to xgettext.
XGETTEXT_OPTIONS = --keyword=_ --keyword=N_
EOF
  fi
else
  $GETTEXTIZE --copy --force >tmpout || exit 1
  if test -f po/ChangeLog~; then
    rm -f po/ChangeLog
    mv po/ChangeLog~ po/ChangeLog
  fi
fi

rm -f aclocal.m4
if test -f `aclocal --print-ac-dir`/gettext.m4; then
  : # gettext macro files are available to aclocal.
else
  # gettext macro files are not available.
  # Find them and copy to a local directory.
  # Ugly way to parse the instructions gettexize gives us.
  m4files="`cat tmpout | sed -n -e '/^Please/,/^from/s/^  *//p'`"
  fromdir=`cat tmpout | sed -n -e '/^Please/,/^from/s/^from the \([^ ]*\) .*$/\1/p'`
  rm -rf gettext.m4
  mkdir gettext.m4
  for i in $m4files; do
    cp -f $fromdir/$i gettext.m4
  done
  ACLOCAL_INCLUDES="-I gettext.m4"
fi

if test -d m4; then
  # gettextize 0.11.4 wants this for some reason
  ACLOCAL_INCLUDES="$ACLOCAL_INCLUDES -I m4"
fi

rm -f tmpout $GETTEXTIZE_FIXED

# Some old version of GNU build tools fail to set error codes.
# Check that they generate some of the files they should.

echo "Running $ACLOCAL..."
$ACLOCAL $ACLOCAL_INCLUDES $ACLOCAL_FLAGS || exit 1
test -f aclocal.m4 || \
  { echo "aclocal failed to generate aclocal.m4" 2>&1; exit 1; }

echo "Running $AUTOHEADER..."
$AUTOHEADER || exit 1
test -f config.h.in || \
  { echo "autoheader failed to generate config.h.in" 2>&1; exit 1; }

echo "Running $AUTOCONF..."
$AUTOCONF || exit 1
test -f configure || \
  { echo "autoconf failed to generate configure" 2>&1; exit 1; }

# Workaround for Automake 1.5 to ensure that depcomp is distributed.
echo "Running $AUTOMAKE..."
$AUTOMAKE -a src/Makefile || exit 1
$AUTOMAKE -a || exit 1
test -f Makefile.in || \
  { echo "automake failed to generate Makefile.in" 2>&1; exit 1; }

) || exit 1

echo Running $srcdir/configure "$@" ...
$srcdir/configure --cache-file=config.cache "$@" && \
  echo "Now type \`make' (\`gmake' on some systems) to compile giFToxic."
