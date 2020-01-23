<?php

$section   = "DEV";
$page      = "todo.php";
$page_desc = "TODO";

require ("../layout.php");

layout_header ($section, $page, $page_desc);

///////////////////////////////////////////////////////////////////////////////

?>

<?php layout_section ("TODO"); ?>
<pre>
<?php

print htmlspecialchars(join("", file("TODO")));

?>
</pre>

<?php

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
