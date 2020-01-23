<?php

///////////////////////////////////////////////////////////////////////////////

require ('site.php');

layout_header ("DOWNLOAD", "Download");

///////////////////////////////////////////////////////////////////////////////

?>

<?php layout_section ("download.c"); ?>

<p>
 Since giFT is still under heavy development, there are no "current" released
 files. The old releases that are available are basically not the same
 project, simply using the same name.  After jasta "tookover" development, the
 project has significantly shifted in direction and has made no formal public
 releases since then.  This is due to the extremely volatile nature of the
 project and we feel it would be best to withhold releases until some more
 APIs and protocol specifications mature and eventually freeze.  However, with
 that said, we still do provide the old giFT releases for educational
 purposes.  Please note that the old giFT <em>used</em> to connect to FastTrack, but
 due to changes made on FastTrack's part, the client was kicked off the
 network and thus currently cannot connect.  The modern giFT now utilizes
 an open source reintrepetation of FastTrack's ideas.
</p>

<dl>

 <dt><h2 class="bullet">CVS/</h2></dt>
 <dd>
  Please see the <a href="docs.php?document=install.html">install guide</a>
  for complete instructions on how to retrieve, build, and use giFT from
  our CVS repository (graciously hosted by sourceforge.net).  Please make
  sure you read carefully as many of the potential problems you will face
  are not literally covered in the install guide (it is impossible to predict
  what older/broken versions of the autotools will do).
 </dd>

 <dt><h2 class="bullet">Releases</h2></dt>
 <dd>
  This section has not yet been completed, but hopefully will be soon once
  eelco gets off his lazy ass and starts helping out again.  Yes eelco,
  I'm talking to YOU! :)
 </dd>

</dl>

<?php

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
