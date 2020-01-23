.\" roff source for "The giFTcurs README."
.\"
.\" Copyright (C) 2003, 2004 Goran Weinholt <weinholt@dtek.chalmers.se>.
.\"
.\" Copying and distribution of this file, with or without modification,
.\" are permitted in any medium without royalty provided the copyright
.\" notice and this notice are preserved.
.\"
.\" $Id: README.ms,v 1.13 2004/01/11 19:35:18 weinholt Exp $
.ss 12 0
.ie n .nr LL 75m
.el   .nr LL 6.5i
.so giftcurs.tmac
.
.TL
The giFTcurs README
.AU
Christian H\[:a]ggstr\[:o]m <chm@c00.info>
G\[:o]ran Weinholt <weinholt@dtek.chalmers.se>
.
.Sh INTRODUCTION
.LP
\fIgiFTcurs\fP is a curses based interface to the \fIgiFT\fP daemon,
a file sharing system.
It allows you to perform searches and control transfers and uploads.
It supports multiple simultaneous searches, filtering, viewing meta
data, customized ordering of search results and transfers, customized
listing of search results and transfers, and much more.
.LP
The current version number can be found in the NEWS file.
\fIgiFTcurs\fP is Free Software and is licensed under the GNU GPL.
See the file \fBCOPYING\fP for the exact copying conditions.
.
.Sh INSTALLATION
.LP
See the file \fBINSTALL\fP for installation instructions.
.LP
If things go well you should be able to run \fIgiFTcurs\fP and connect to
the \fIgiFT\fP daemon.
If you don't have \fIgiFT\fP, look at http://giftproject.org/.
.LP
The latest \fIgiFTcurs\fP release should always work with the latest release
of \fIgiFT\fP.
Newer versions of \fIgiFT\fP might have changes in the interface protocol.
If things don't work then you might need to get a newer version of
\fIgiFTcurs\fP, visit http://www.nongnu.org/giftcurs/.
.
.Sh CONTACTING THE DEVELOPERS / GETTING SUPPORT
.LP
You can find us in #giFTcurs on irc.freenode.net.
These mailing lists are available:
.RS
.IP \fBgiFTcurs\-user\fP 18n
discussion between users
.IP \fBgiFTcurs\-devel\fP
discussion between developers
.IP \fBgiFTcurs\-commits\fP
changes to the CVS
.RE
.LP
Go to http://savannah.nongnu.org/mail/?group=giftcurs to subscribe or see
the list archives.
See the AUTHORS file if you want to contact one of us personally.
.
.Sh CONTRIBUTING
.LP
If you wish to contribute to \fIgiFTcurs\fP, you should build it from
CVS, follow the \fBgiFTcurs\-commits\fP mailing list, and at least
subscribe to \fBgiFTcurs\-devel\fP (the list isn't very active yet).
The IRC channel is also a place where it's nice to be, since you can
talk to other developers much easier, and get instant notification of
commits to the CVS.
.LP
Patches should preferably be in unified context format.
For indentation rules, please refer to src/.indent.pro and \fIindent\fP.
Updates to the .po files should preferably be sent not as patches,
but as whole files.
Send your contributions to the \fBgiFTcurs\-devel\fP list.
.
.Sh BUILDING FROM CVS
.LP
When reporting a bug, please try to make sure that it is present in the
CVS version of \fIgiFTcurs\fP.
Run this to get the CVS source:
.Vs
export CVS_RSH=ssh
cvs \-d:ext:anoncvs@savannah.gnu.org:/cvsroot/giftcurs co giFTcurs
.Ve
Then whenever you want to update the source, just run
\[lq]cvs \-z3 up \-Pd\[rq] in the giFTcurs directory.
To compile \fIgiFTcurs\fP from CVS you must have the GNU autotools (automake,
autoconf), the ncurses headers, gettext and GLib 2.
A fairly recent (think 0.10.40) version of gettext is required, since we use
ngettext.
With some operating systems you'll need the package gettext\-devel as well.
To compile type this:
.Vs
\&./autogen.sh
make
su
make install
.Ve
A common problem is parse errors in the \fIpo/\fP directory, if this
happens you should try to run \[lq]cvs up \-C\[rq] first.
If this doesn't help, you probably have a broken gettext installation.
.
.Sh PORTABILITY
.LP
\fIgiFTcurs\fP is known to work on the following systems:
.IP
.\" Sort this list alphabetically. Comments say who confirmed portability.
AmigaOS 3.0,      \" Diego Casorran
Cygwin,           \" rasa
Debian GNU/Linux, \" weinholt
FreeBSD 5.1,      \" jmitchell
Mac OS X,         \" unknown person. find someone
NetBSD 1.6.1,     \" weinholt
and
Solaris.          \" weinholt
.LP
Versions prior to \fIgiFTcurs\fP 0.6.0 worked with at least these systems:
.IP
AIX, BeOS, Debian GNU/Hurd, Debian GNU/Linux bo hamm slink potato woody,
FreeBSD 4.4, Mac OS X, NetBSD 1.5.2, OpenBSD 3.0, SunOS, Solaris,
Tru64 (OSF1 V4.0), and cygwin.
.LP
Patches/suggestions which fix portability issues are highly appreciated.
.
.Sh SEE ALSO
.LP
.I giFTcurs (1),
.I giFTcurs.conf (5).
