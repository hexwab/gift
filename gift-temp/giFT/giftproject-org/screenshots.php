<?php

///////////////////////////////////////////////////////////////////////////////

require ('site.php');

layout_header ("SCREENSHOTS", "Screenshots");

///////////////////////////////////////////////////////////////////////////////

?>

<?php layout_section ("eye_candy/"); ?>
<table border="0" width="100%">
<?php

$ss = array ("giftbox-search" => "Quite clearly the sexiest client",
			 "fe-search"      => "Illustrating the network's obvious " .
                                 "abundance of the best trance ever created",
             "fe-transfer"    => "Hi, my name is bobbi, I'm 18 years and " .
                                 "old, I'm from Colorado and today ...",
             "curs-download"  => "curses is sexy as well",
             "gift-win32"     => "I think the ugly desktop speaks for " .
                                 "itself on this one");

foreach ($ss as $file => $desc)
{
?>
 <tr>
  <td valign="top"><?php echo $desc; ?></td>
  <td><br></td>
  <td align="right">
   <a href="/resources/images/screenshots/<?php echo $file; ?>.jpg">
	<img alt="<?php echo $file ?>" src="resources/images/screenshots/<?php echo $file; ?>-thumb.jpg">
   </a>
  </td>
 </tr>
 <tr><td colspan="2"><br></td></tr>
<?php
}

?>
</table>

<?php

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
