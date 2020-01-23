<?php

///////////////////////////////////////////////////////////////////////////////

$prefix = $site_http;

// lookup array for the menu function
$sec_lookup =
array (
 "HOME" =>
    array ("main"               => "$prefix/"),
 "NEWS" =>
    array ("main"               => "$prefix/news.php"),
 "DOCUMENTATION" =>
    array ("main"               => "$prefix/docs.php",
           "faq"                => "$prefix/docs.php?document=faq.html",
           "install"            => "$prefix/docs.php?document=install.html",
           "interface"          => "$prefix/docs.php?document=interface.html",
           "overview"           => "$prefix/docs.php?document=whatis.html"),
 "DOWNLOAD" =>
    array ("main"               => "$prefix/download.php"),
 "CLIENTS" =>
    array ("main"               => "$prefix/clients.php"),
 "SCREENSHOTS" =>
    array ("main"               => "$prefix/screenshots.php"),
 "CONTACT" =>
    array ("main"               => "$prefix/contact.php")

);

///////////////////////////////////////////////////////////////////////////////

// <h1> basically ;)
function layout_section ($title)
{
	// i like this :)
	// $color_bg = "#f4f4f7";

	if ($color_bg)
	{
		echo "<p><table border=0 width=100%>\n";
		echo "<tr><td bgcolor=$color_bg>";
	}

	echo "<h1>$title</h1>";

	if ($color_bg)
	{
		echo "</td></tr>\n";
		echo "</table></p>";
	}

	echo "\n";
}

///////////////////////////////////////////////////////////////////////////////

function layout_header ($section, $page_desc, $page = null)
{
	global $view;
	global $prefix;

	if ($page === null)
		$page = $PHP_SELF;

	// there's gotta be a better way to do this :)
	// yeah, by setting some global vars... ;)
	$images  = "$prefix/resources/images";
	$css     = "$prefix/resources/style.css";

?>
<html>
 <head>
  <title>giFT: <?php echo $page_desc; ?></title>
  <link rel="stylesheet" type="text/css" href="<?php echo $css; ?>">
 </head>
 <body marginwidth="0" bgcolor="#788897">
 <table border="0" cellspacing="0" cellpadding="0" width="793" align="center">
  <tr>
   <td><img src="<?php echo $images; ?>/spacer.gif" width="3" height="1" alt=""></td>
   <td><img src="<?php echo $images; ?>/spacer.gif" width="703" height="1" alt=""></td>
   <td><img src="<?php echo $images; ?>/spacer.gif" width="3" height="1" alt=""></td>
   <td><img src="<?php echo $images; ?>/spacer.gif" width="84" height="1" alt=""></td>
  </tr>
  <tr>
   <td colspan="4"><img src="<?php echo $images; ?>/top.jpg" alt="gift"></td>
  </tr>
  <tr>
   <td bgcolor="#000000"><img src="<?php echo $images; ?>/spacer.gif" width="3" height="1" alt=""></td>
   <td bgcolor="#ffffff">
    <table border="0" cellpadding="5" width="100%">
	 <tr><td>
<?php

	layout_menu ($section, $page, $page_desc);

	echo "<!-- BEGIN BODY -->\n";

	if ($view == "source")
	{
		layout_footer ($page, $page_desc);

		// don't let the actual html be displayed
		exit ();
	}
}

///////////////////////////////////////////////////////////////////////////////

function layout_menu ($section, $page, $page_desc)
{
	global $view;
	global $sec_lookup;

?>
<!-- BEGIN MENU -->
<center>
<table align="center" border="0" cellspacing="0" cellpadding="2"><tr>
<?php

    $viewurl = $view ? "?view=$view" : "";

	$sec = array_keys ($sec_lookup);
	for ($i = 0; $i < count($sec); $i++)
	{
		$filename = $sec_lookup[$sec[$i]];

		echo "<td><span class=\"menu\"> ::: ";

		if ($section == $sec[$i])
		{
			if ($page != $filename[main])
				echo "<a class=\"current\" href=\"$filename[main]$viewurl\">" . strtolower ($sec[$i]) . "</a>";
			else
				echo "<span class=\"current\">" . strtolower ($sec[$i]) . "</span>";

			$sub_menu = $filename;
        	$tdnr = $i;
		}
		else
		{
			echo "<a href=\"$filename[main]$viewurl\">" . strtolower ($sec[$i]) . "</a>";
		}

		if (count($sec) - $i > 1)
			echo "</span></td>\n";
	}

	echo " ::: </span></td>\n";

?>
</tr>
<?php

	if (count ($sub_menu) > 1)
	{
		echo "<tr>";

		if ($tdnr > 0)
			echo "<td colspan=\"" . $tdnr  . "\">&nbsp;</td>";

		echo "<td colspan=\"" . (count ($sec) - $tdnr) . "\">";

		array_shift ($sub_menu);
		foreach ($sub_menu as $sub => $filename)
		{
			echo "<span class=\"menu\">: ";

			if ($page == $filename)
				echo "<span class=\"current\">$sub</span>";
			else
				echo "<a href=\"$path$filename$viewurl\">$sub</a>";

			echo " :</span>";
		}
		echo "</td></tr>";

	}

?>
</table>
</center><br>
<!-- END MENU -->
<?php
}

///////////////////////////////////////////////////////////////////////////////

function layout_footer ($page, $page_desc)
{
	global $view;
	global $HTTP_SERVER_VARS;
	global $PHP_SELF;

	if ($view == "source")
		highlight_file ($HTTP_SERVER_VARS["SCRIPT_FILENAME"]);

	$images = "/resources/images";

?>
<!-- END BODY -->
<!-- BEGIN FOOTER -->
<br><br>
<table border="0" width="100%">
 <tr>
  <td align="left">
<?php
	if ($view == "source")
		echo "<a href=\"$PHP_SELF\">hide source</a>\n";
	else
		echo "<a href=\"$PHP_SELF?view=source\">show source</a>\n";
?>
  </td>
  <td align="right"><a href="http://sourceforge.net/projects/gift"><img src="http://sourceforge.net/sflogo.php?group_id=34618" border="0" alt="sourceforge logo"></a></td>
 </tr>
</table>
<!-- END FOOTER -->
     </td></tr>
    </table>
   </td>
   <td bgcolor="#000000"><img src="<?php echo $images; ?>/spacer.gif" width="3" height="1" alt=""></td>
   <td background="<?php echo $images; ?>/bg.gif" valign="top" rowspan="2"><img src="<?php echo $images; ?>/upperrightcorner.jpg" alt=""></td>
  </tr>
  <tr>
   <td bgcolor="#000000" colspan="3"><img src="<?php echo $images; ?>/spacer.gif" width="3" height="3"></td>
  </tr>
 </table>
 </body>
</html>
<?php
}

/* vim:ts=4:noet: */

?>
