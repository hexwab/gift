<?php

///////////////////////////////////////////////////////////////////////////////

require ('site.php');

layout_header ("HOME", "Internet File Transfer", "$prefix/");

///////////////////////////////////////////////////////////////////////////////

?>

<?php layout_section ("README"); ?>
<p>
 What is giFT, you ask?  giFT is a modular daemon capable of abstracting the
 communication between the end user and specific filesharing protocols
 (peer-to-peer or otherwise).  The giFT project differs from many other
 similar projects in that it is a distribution of a standalone
 (platform-independent) daemon, a library for client/frontend development, and
 our own homegrown network OpenFT.
</p>

<p>
 The goal of this project is to ensure that user interface developers never
 waste time with the low-level details of a protocol and that network
 programmers never waste time with the user interface details.  Along those
 same lines it allows end users to control which protocols they use no matter
 which giFT interface they have selected as their favorite.  Any new
 filesharing network can be supported without any change to the user
 interface.  Cool, huh?
</p>

<dl>

 <dt><h2 class="bullet">src/</h2></dt>
 <dd>
<p>
 The most important component of the giFT project is the daemon.  The giFT
 daemon acts as a "bridge" between multiple backend file sharing protocols,
 exposing them to the end user interface developer in an easy to manage
 text-based protocol.  Documentation may be found
 <a href="docs.php?document=interface.html">here</a>.
</p>
 </dd>

 <dt><h2 class="bullet">OpenFT/</h2></dt>
 <dd>
<p>
 OpenFT is a peer-to-peer network designed to exploit all the functionality
 giFT supports.  Loosely based on FastTrack's design, OpenFT aims to become
 the new pseudo standard in file trading on the Internet, but we'll settle for
 Total World Domination.
</p>
 </dd>

</dl>

<?php

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
