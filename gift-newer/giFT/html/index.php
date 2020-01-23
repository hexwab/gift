<?php

require ("layout.php");

$section   = "HOME";
$page      = "$prefix/";
$page_desc = "Internet File Transfer";

layout_header ($section, $page, $page_desc);

///////////////////////////////////////////////////////////////////////////////

?>

<?php layout_section ("README"); ?>
<p>What is giFT, you ask?  giFT is an acronym for "giFT: Internet File Transfer".
The giFT project is an initiative to attempt to unify the
divided peer-to-peer community following Napster's demise.</p>

<p>The basic underlying concept of giFT is that there should be no direct
connection between the user interface preferred by the user, and the back-end
protocol.  This is tackled using a collection of several components together:</p>

<dl>
 <dt><h2 class="bullet">src/</h2></dt>
 <dd>
<p>The giFT daemon acts as a "bridge" between multiple backend file sharing
protocols, exposing them to the end developer in an easy to understand XML-like
<a href="docs/?document=interface.html"> interface protocol</a>.</p>

<p>Yes, I know what you're thinking "hey, that sounds a lot like Jabber!".  Well,
you're partly correct.  Jabber worked by setting up a finite number of
translation servers on the Internet, requiring the user to authenticate with
one extra remote server in order to take advantage of this technology.</p>

<p>We feel that the task would be better handled by a local daemon that acts
transparently to the user, feeding the benefits solely to the developer.  The
giFT team believes that the best way to improve the state of file sharing on
the Internet is to allow developers to take on the complex (and unique) tasks
specific to their project, rather than re-inventing the wheel that each
interface and network must have.</p>
 </dd>
 <dt><h2 class="bullet">OpenFT/</h2></dt>
 <dd>
<p>OpenFT is a p2p network designed to exploit all the functionality giFT
supports.  Loosely based on FastTrack's design, OpenFT aims to become the new
pseudo standard in file trading on the Internet, but we'll settle for Total
World Domination.</p>
 </dd>
 <dt><h2 class="bullet">ui/</h2></dt>
 <dd>
<p>The default giFT front-end is a GTK application to interact with the daemon's
interface protocol.  Multiple instances of giFT-fe can be launched from
multiple computers to interact with a single daemon, thus requiring only one
instance of the core translation service per network.  This adds some
flexibility at the cost of complexity.</p>

<p>Most giFT users will run the daemon on the same computer as giFT-fe, and the
front-end will likely have features designed to make it easier for them to do
just that at startup.  However, the daemon does not require any clients to
maintain a connection to the network.  This allows any user to essentially
"screen" downloads or communication on any protocol that giFT supports while
restarting the preferred front-end.</p>
 </dd>
</dl>

<?php

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
