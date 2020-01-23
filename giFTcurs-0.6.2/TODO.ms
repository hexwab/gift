.\" roff source for "The giFTcurs TODO."
.\"
.\" Copyright (C) 2004 Goran Weinholt <weinholt@dtek.chalmers.se>.
.\"
.\" Copying and distribution of this file, with or without modification,
.\" are permitted in any medium without royalty provided the copyright
.\" notice and this notice are preserved.
.\"
.\" $Id: TODO.ms,v 1.1 2004/01/01 16:11:54 weinholt Exp $
.ss 12 0
.ie n \{\
. na
. nr HY 0
. nr LL 75m
.\}
.el .nr LL 6.5i
.so giftcurs.tmac
.
.TL
The giFTcurs TODO
.
.Sh STUFF LIKELY TO BE DONE
.RS
.IP \(bu 2m
A debugging key.
You press it and it dumps info about the marked item in the console.
.IP \(bu
Hilight new transfers.
.IP \(bu
Ability to tag search results and perform an action on all tagged ones.
.IP \(bu
Ability to search on meta data.
.IP \(bu
Ability to choose filename for downloads.
.IP \(bu
More documentation (e.g., a FAQ, something documenting hidden features.)
.IP \(bu
A screen where you browse users.
.IP \(bu
Numbered links, as in lynx.
Like say you move on page down with space and then you see foobar.ogg
as entry 39 and you press 3 and 9 and then enter.
Request from b0ef.
.IP \(bu
Move away from glib's logging facility, since it has a silly buffer
that is 1025 bytes in size. We need more.
(Maybe this isn't needed, what with the new interface protocol and all.)
.IP \(bu
Mark some more strings for translation.
The sort methods aren't being translated right now.
.IP \(bu
Download whole directories with the push of a button.
The files would be saved in a seperate directory in completed/.
.RE
.\" Not to be implemented stuff here:
.\"  . A screen where you browse users.
.\"    The browse feature is to be removed from giFT anyway.
.\"  . A toolkit for the interface.
.\"    This toolkit hasn't been developed for some time.
.\"  . More than one predefined color theme.
.\"  . Steal the formatting stuff principle from irssi.
.\"  . Trough scroll indicator.
.\"  . Reset the status bar to 'This is free ..' after a time.
.\"    Was implemented for a while but later removed. Deemed bad.
.\"  . A "boss key" (^B) that makes giFTcurs forget everything :)
.\"  . Ability to execute a command when a download finishes.
.\"    Really belongs in giftd space, or in a minimal client.
.\" Implemented stuff:
.\"  . A screen where you see the log, maybe chat could be here as well.
.\"  . Removing auto-wakeup of downloads (a small prog running in
.\"    background is probably better)
.\"  . Sorting of download list (largest filesize to smallest)
.\"  . Clickable areas to switch between screens.
.\"  . Pause transfers.
.\"  . Save searches and have a list where you can choose between them.
.\"  . Some vi-style editing.
.\"  . Better ETA code.
.\"  . An option to auto-clear finished transfers.
.\"  . List of banned words.
.\"  . The history code for the search box is very broken.
.\"  . Have the transfer screen use a timer. Having lots of stuff
.\"    there as it is now makes giFTcurs very unresponsive.
.\"  . Pressing home in the search query box does something odd.
.\"  . / for searching among results.
.\"  . Better mouse support.
.
.
.Sh UNLIKELY STUFF
.RS
.IP \(bu 2m
World domination and so on.
.IP \(bu
Client to client chat.
.IP \(bu
Use readline(3) for the input fields.
This is probably not a good idea, and not even needed anymore.
.RE
.
.LP
Never forget, we must fight the feature creep!
