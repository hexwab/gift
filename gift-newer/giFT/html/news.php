<?php

$section   = "NEWS";
$page      = "/news.php";
$page_desc = "News";

require ("layout.php");

layout_header ($section, $page, $page_desc);

///////////////////////////////////////////////////////////////////////////////

?>

<?php layout_section ("news/"); ?>
<?php

include ("http://sourceforge.net/export/projnews.php?" .
         "group_id=34618&limit=5&flat=0&show_summaries=1");

?>

<?php

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
