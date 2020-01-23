<?php

require ("../layout.php");

$section   = "DEV";
$page      = "$prefix/dev/";
$page_desc = "Internet File Transfer";

layout_header ($section, $page, $page_desc);

///////////////////////////////////////////////////////////////////////////////

?>

<?php layout_section ("dev/"); ?>

<p>giFT can use some extra developers. Go to #giFT on irc.openprojects.net if you
think you have the skills we need. Also, just grepping for TODO in the source
files is never a bad idea.</p>

<p>If you want to develop a GUI, please check out <a href="clients.php">all
existing front ends</a> first to see if you can help. All frontends will be
listed here, somewhere in the future... If you really want to develop your own
frontend, read the <a href="../docs/?document=interface.html">Interface
Protocol</a> documentation.</p>

<?php

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
