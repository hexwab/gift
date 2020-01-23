<?php

require ("../layout.php");

$section = "DOCUMENTATION";

///////////////////////////////////////////////////////////////////////////////

$docdir = "../resources/docs/";
$docarr = array();

// lookup all documentation in the docs/ dir
$dir = opendir ($docdir);

while ($dirent = readdir ($dir))
{
	if ($dirent == "." || $dirent == "..")
		continue;

	// make sure we're only dealing with HTML
	if (!preg_match ("/\.html$/", $dirent))
		continue;

	// read in the first part of the file to look for a valid doc entry
	$fp = fopen ("$docdir/$dirent", "r");
	$data = fread ($fp, 256);
	fclose ($fp);

	preg_match ("/^<!-- (.*?): (.*?) -->/", $data, $matches);

	// found a doc-link
	if ($matches[1] && $matches[2])
	{
		$docarr[$dirent] = array ($matches[1], $matches[2]);
	}
}

closedir ($dir);

///////////////////////////////////////////////////////////////////////////////

if (isset ($docarr[$document]))
{
	$page      = "?document=$document";
	$page_desc = $docarr[$document][0];
}
else
{
	$page      = "$prefix/docs/";
	$page_desc = "Documentation";
}

layout_header ($section, $page, $page_desc);

///////////////////////////////////////////////////////////////////////////////

// we were instructed to load a specific document, not the regular doc page
if (isset ($docarr[$document]))
{
	include ("$docdir/$document");
}
else
{
?>
<?php layout_section ("doc/"); ?>
<p>Current documentation available:
 <ul>
<?php
    
    foreach ($docarr as $doc => $descr)
    {
        echo "  <li><a href=\"?document=$doc\">$descr[0]</a> - $descr[1]</li>\n";
    }
?>
 </ul>
</p>
<p>
The documents mentioned here are also included in the CVS repository for giFT.
Try looking in the doc/ directory.  The docs here are usually auto-generated
from the sources there.
</p>
<p>
If the documentation can't seem to answer your question feel free to stop by
our official irc channel: <a href="http://www.openprojects.net/">irc.openprojects.net</a>:#gift.  
Please be sure you read the documentation carefully before you ask questions
there, however.
</p>
<br>
<?php
}

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
