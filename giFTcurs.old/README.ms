.\" $Id: README.ms,v 1.3 2003/05/16 23:03:37 weinholt Exp $
.\" giFTcurs README File source.
.\" Brian Mays' debian/README.ms from the rxvt package was used as a template.
.ss 12 0 \" No extra space after sentences.
.ie n \{ \"	If nroff'ed
.nr LL 75m \"	    Width = 75 chars
.na \"		    No justification
.nr HY 0 \"	    No hyphenation
\}
.el \{ \"	Else
.nr LL 6.5i \"	    Line length 6 1/2"
\}
.de Vs \"	Start verbatim environment
.DS \"		    Indent block
'if t \{
'ft CW \"	    Courier constant width font
\}
..
.de Ve \"	End verbatim environment
'if t \{
'R \"		    Roman font
\}
.DE \"		    End indent
..
.TL \" Title
giFTcurs README File
.AU \" Authors
Christian H\[:a]ggstr\[:o]m <chm@c00.info>
G\[:o]ran Weinholt <weinholt@dtek.chalmers.se>
.SH \" new section
INSTALLATION
.LP
To compile \fIgiFTcurs\fP you need to have the development headers for
\fIncurses\fP and \fIglib\-2.0\fP installed.
\fIpkg\-config\fP is also required.
When you've untarred the sources, cd into the \fIgiFTcurs\fP directory
and type:
.Vs
\&./configure
make
su
make install
.Ve
If things went well you should be able to run \fIgiFTcurs\fP and connect to
the \fIgiFT\fP daemon.
If you don't have giFT, look at http://giFT.sf.net/.
.LP
Currenly \fIgiFTcurs\fP only works with the CVS version of \fIgiFT\fP.
This is because the interface protocol changes frequently.
If things don't work then maybe you need to get a newer version of
\fIgiFTcurs\fP, visit http://giFTcurs.sf.net/.
.SH \" new section
CONTACTING THE DEVELOPERS / GETTING SUPPORT
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
Go to http://fnord.csbnet.se/pipermail to subscribe or see
the list archives.
See the AUTHORS file if you want to contact one of us personally.
.SH \" new section
BUILDING FROM CVS
.LP
When reporting a bug, please try to make sure that it is present in the
CVS version of \fIgiFTcurs\fP.
Run this to get the CVS source:
.Vs
export CVSROOT=:pserver:anonymous@cvs.sf.net:/cvsroot/giftcurs
cvs login
cvs \-z3 co giFTcurs
.Ve
Then whenever you want to update the source, just run "cvs \-z3 up \-Pd"
in the giFTcurs directory.
To compile \fIgiFTcurs\fP from CVS you must have the GNU autotools (automake,
autoconf), the ncurses headers, gettext and GLib 2.
A fairly recent (think 0.10.40) version of gettext is required, since we use
ngettext.
With some distributions you'll need the package gettext\-devel as well.
To compile type this:
.Vs
\&./autogen.sh
make
su
make install
.Ve
A common problem is parse errors in the \fIpo/\fP directory, if this
happens you should try to run "cvs up \-C" first.
If this doesn't help, you probably have a broken gettext installation.
.SH \" new section
PORTABILITY
.LP
\fIgiFTcurs\fP is known to work on the following systems:
.IP
Debian GNU/Linux,
Mac OS X,
Solaris,
and cygwin.
.LP
Versions prior to \fIgiFTcurs\fP 0.6.0 worked with at least these systems:
.IP
AIX, BeOS, Debian GNU/Hurd, Debian GNU/Linux bo hamm slink potato woody,
FreeBSD 4.4, Mac OS X, NetBSD 1.5.2, OpenBSD 3.0, SunOS, Solaris,
Tru64 (OSF1 V4.0), and cygwin.
.LP
Patches/suggestions which fix portability issues are highly appreciated.
.SH
SEE ALSO
.LP
.I giFTcurs (1),
.I giFTcurs.conf (5),
.I
The giFTcurs FAQ
