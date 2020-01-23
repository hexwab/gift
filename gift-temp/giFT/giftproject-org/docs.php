<?php

///////////////////////////////////////////////////////////////////////////////

require ('site.php');

$section = "DOCUMENTATION";

///////////////////////////////////////////////////////////////////////////////

$docdir = "resources/docs/";
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
	$page      = "$prefix/docs.php";
	$page_desc = "Documentation";
}

layout_header ($section, $page_desc, $page);

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
<p>Current documentation available:</p>

<dl>
 <dd>
  <table border="0" width="100%">
<?php

	foreach ($docarr as $doc => $descr)
	{
?>
  <tr>
   <td width="25%" valign="top">
    <li><a href="<?= $PHP_SELF ?>?document=<?= $doc ?>"><?= $descr[0] ?></a>
   </td>
   <td>
    <?php echo $descr[1]; ?>
   </td>
  </tr>
<?php
	}

?>
  </table>
 </dd>
</dl>

<p>
The documents mentioned here are also included in the CVS repository for giFT.
Try looking in the doc/ directory.  The docs here are usually auto-generated
from the sources there.  Please note that if you are unable to find the
solution to your problem through this documentation you can still attempt to
contact the project members / groupies by heading over to the
<a href="<?= $GLOBALS[prefix] ?>/contact.php">contact page</a>.  Be
sure you select the appropriate channel for communication, lest you will be
flamed uncontrollably.
</p>
<br>
<?php
}

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
