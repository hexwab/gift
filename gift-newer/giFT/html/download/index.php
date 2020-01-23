<?php

require ("../layout.php");

$section   = "DOWNLOAD";
$page      = "$prefix/download/";
$page_desc = "Download";

layout_header ($section, $page, $page_desc);

///////////////////////////////////////////////////////////////////////////////

?>

<?php layout_section ("download/"); ?>

<p>Since giFT is still under heavy development, there are no released files.
There are some old ones, but they won't work anymore, because they're the
<b>old</b> giFT, which connected to the FastTrack network.</p>

<p>However, you <b>can</b> try giFT, if you're able do the compiling yourself.
You can get the <a href="cvs.php">most recent version</a> from CVS. Please read
the <a href="../docs/?document=install.html">Installation Guide</a> for more
information. The Installation Guide also covers most (if not all) problems you
can run into, so it's a <b>must</b> read.</p>

<?php

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
