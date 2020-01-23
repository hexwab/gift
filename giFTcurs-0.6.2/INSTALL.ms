.\" roff source for "The giFTcurs Installation Instructions."
.\"
.\" Copyright (C) 2003, 2004 Goran Weinholt <weinholt@dtek.chalmers.se>.
.\"
.\" Copying and distribution of this file, with or without modification,
.\" are permitted in any medium without royalty provided the copyright
.\" notice and this notice are preserved.
.\"
.\" $Id: INSTALL.ms,v 1.6 2004/01/01 16:07:49 weinholt Exp $
.ss 12 0
.ie n .nr LL 75m
.el   .nr LL 6.5i
.so giftcurs.tmac
.
.TL
The giFTcurs Installation Instructions
.AU
Christian H\[:a]ggstr\[:o]m <chm@c00.info>
G\[:o]ran Weinholt <weinholt@dtek.chalmers.se>
.
.Sh GENERIC INSTALLATION
.LP
To compile \fIgiFTcurs\fP you need to have the development headers for
\fIncurses\fP and \fIglib\-2.0\fP installed.
\fIpkg\-config\fP is also required.
If \fIglib\-2.0\fP is installed in a non-standard location, you might
need to set the \fIPKG_CONFIG_PATH\fP environment variable to the
directory where \fIglib\-2.0.pc\fP is.
When you've untarred the sources, cd into the source directory and type:
.Vs
\&./configure
make
su
make install
.Ve
If things went well you should be able to run \fIgiFTcurs\fP and connect to
the \fIgiFT\fP daemon.
If you don't have giFT, look at http://giftproject.org/.
.
.Sh CONFIGURE FLAGS
.\" This section should document the flags we add ourselves.
.LP
Here are some flags you can pass to configure in order to enable or disable
features, or change what libraries are used:
.LP
.B \-\-with\-ncurses[=dir]
.IP
Build with ncurses.
If \fBdir\fP is specified, the configure script will try to use the
ncurses library in that directory.
.LP
.B \-\-with\-ncursesw[=dir]
.IP
Build with ncursesw.
The configure script will check if the \fIwadd_ch\fP function is available
and in that case enable UTF-8 support.
.LP
.B \-\-disable\-libgpm
.IP
By default the configure script will look for libgpm, to get mouse
support in the Linux console.
This flag turns off this check.
.LP
.B \-\-disable\-mouse
.IP
This flag turns off mouse support.
This flag exists just so that people who don't need mouse support will
get a smaller binary.
.LP
.B \-\-disable\-internal\-mouse
.IP
If mouse support is enabled, it will by default be handled by routines
built-in to \fIgiFTcurs\fP.
This internal mouse support can use libgpm and xterm mouse codes, but
it will not work with e.g. OS/2 terminals.
.LP
There are more flags you can pass to configure. Use the \fB\-\-help\fP
flag to get a list.
.
.Sh INSTALLATION IN CYGWIN
.\" Maintained by rasa.
.LP
To install \fIgiFTcurs\fP for \fIcygwin\fP, you will need to download and
install \fIglib\fP.
.\" FIXME: is this location canoncical? It's also rather overlong down there.
Browse http://web.sfc.keio.ac.jp/~s01397ms/cygwin/ and download the latest
version of glib, and unpack it in cygwin's root directory.
Provided that the version is 2.2.1\-2, the following will do this:
.Vs
wget http://web.sfc.keio.ac.jp/~s01397ms/cygwin/glib\-2.2.1\-2.tar.gz \\
 \-O \- | tar \-C / \-xvzf \-
.Ve
Now follow the instructions in the GENERIC INSTALLATION section.
.
.Sh INSTALLATION ON DEBIAN GNU/LINUX 3.0
.\" Maintained by weinholt.
.LP
GLib 2 isn't available for \fIDebian GNU/Linux 3.0\fP, so you'll need
to install it.
It's rather annoying to build this yourself, since the glib2.0 package
needs pkg\-config to build, and vice versa.
For your convenience, there's an APT repository at this location, that
has woody backports of gift, giftcurs, glib2.0 and pkg\-config:
.Vs
deb http://fnord.csbnet.se/giftcurs stable main
deb\-src http://fnord.csbnet.se/giftcurs stable main
.Ve
Just put these two lines in /etc/apt/sources.list, run apt\-get update,
and then apt\-get install gift.
.
.Sh INSTALLATION ON DEBIAN TESTING AND SID
.\" Maintained by weinholt.
.LP
\fIgiFTcurs\fP is already available in Debian testing and sid, just run
apt\-get install giftcurs.
.
.Sh INSTALLATION ON NETBSD 1.6
.\" Maintained by weinholt, kinda.
.LP
\fIgiFTcurs\fP has been tested and found working on NetBSD 1.6.1.
The system's curses library will work, but it's recommended that you
use ncurses.
Install the ncurses, glib2, and pkgconfig packages.
For configure to find ncurses, you might need to run it like this:
.Vs
\&./configure \-\-with\-ncurses=/usr/pkg/
.Ve
Now you can follow the instructions in the GENERIC INSTALLATION section.
.
.Sh INSTALLATION ON AMIGAOS
.\" Maintained by weinholt, with info from Diego Casorran.
.LP
At least AmigaOS v3.0 is needed, and to compile \fIgiFTcurs\fP you need
gcc 2.95.3+ with ixemul.library.
It has been tested with AmigaOS v3.5 running on a 68060 with ixemul v48.2.
.LP
.\" Apparently amiga-8bit wasn't perfect though
To get \fIgiFTcurs\fP to show correctly, you should use the terminfo
entry \fBamiga\-8bit\fP.
.LP
You can (soon) download a precompiled version of giFTcurs for AmigaOS from
\fBhttp://amiga.sf.net/\fP.
It has not yet been uploaded at the time of this writing.
.
.Sh SEE ALSO
.LP
.I giFTcurs (1),
.I giFTcurs.conf (5),
.I "The giFTcurs README" .
