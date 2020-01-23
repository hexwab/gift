<?php

$section   = "DOWNLOAD";
$page      = "old.php";
$page_desc = "Download";

require ("../layout.php");

layout_header ($section, $page, $page_desc);

///////////////////////////////////////////////////////////////////////////////

?>

<?php layout_section ("releases/"); ?>
<i>All giFT releases are currently out of date! This section only exists for educational purposes.</i><br><br>
<table border="0">
 <tr>
  <td><b>version</b></td>
  <td><b>date</b></td>
  <td><b>download</b></td>
  <td><b>changelog</b></td>
 </tr>
<?php

$releases = array ("0.9.7" => array ("September 27, 2001", ".tar.gz"),
                   "0.9.6" => array ("September 19, 2001", ".tar.gz"),
                   "0.9.5" => array ("September 12, 2001", ".tgz"));

foreach ($releases as $ver => $extra_info)
{
?>
<tr>
 <td><?php echo $ver; ?></td><td><?php echo $extra_info[0]; ?></td><td align="right"><a href="http://prdownloads.sf.net/gift/giFT-<?php echo "$ver$extra_info[1]" ?>">tarball</a></td><td><a href="#<?php echo $ver ?>">changelog</a></td>
</tr>
<?php
}

?>
</table>

<!-- CHANGELOG -->
<?php layout_section ("ChangeLog") ?>
<pre>
<?php

// if this isn't a clean solution, i don't know what it is :)

$changelog = htmlspecialchars (implode ("", file ("ChangeLog")));
$changelog = preg_replace ("/(^Version (\d\.\d\.\d):)/m", "<a name=\"\\2\">\\1</a>", $changelog);
print $changelog;

?>
</pre>
<!-- END CHANGELOG -->

<?php

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
