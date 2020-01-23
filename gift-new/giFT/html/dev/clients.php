<?php

$section   = "DEV";
$page      = "clients.php";
$page_desc = "Clients";

require ("../layout.php");

layout_header ($section, $page, $page_desc);

///////////////////////////////////////////////////////////////////////////////

?>

<?php layout_section ("clients/"); ?>

<p>'Client', 'front end', 'interface' and 'GUI' are interchangably used, and
all mean the same thing. Namely a program that communicates with giFT's <!-- ' -->
interface protocol.</p>

<?php

// uglyness alert... *sigh*
// yes i screwed the identation on purpose...

$clients =
array (
 "giFTcurs" =>
    array ("http://giftcurs.sourceforge.net",
           "Cursed frontend that has been described as \"seriously slick\".  Currently the best client available at the moment, and therefore recommended"),
/* "giFT-fe" =>
    array ("",
           "Official front-end and is packaged with giFT/OpenFT.  Uses GTK.  Ported to Windows."), */
 "giFT-shell" =>
    array ("",
           "Gnut-like interface to giFT, which is also packaged with giFT/OpenFT."),
 "KCeasy" =>
    array ("http://kceasy.sourceforge.net",
           "Win32 client, including a media player (currently in alpha)."),
 "giFToxic" =>
    array ("http://giftoxic.sourceforge.net",
           "GTK2 client, in development"),
 "Kift" =>
    array ("http://sourceforge.net/projects/kift",
           "KDE/QT interface."),
/* "ewlgiFT" =>
    array ("http://ewlgift.sourceforge.net",
           "A simple client, using e17's ewl widget."), */
 "wiFT" =>
    array ("http://sourceforge.net/projects/wift",
           "A native Win32 client, written in Delphi 6/Kylix.") /*,*/
/* "Kadeau" =>
    array ("http://sourceforge.net/projects/kadeau",
           "Another KDE/QT interface."),*/
/* "QT-giFT" =>
    array ("http://qtgift.sourceforge.net",
           "Yet another KDE/QT client..."), */
/* "php-gtk-giFT" =>
    array ("http://freshmeat.net/projects/php-gtk-gift",
           "PHP-GTK interface."), */ 
/* "giFTopia" =>
    array ("http://www.geocities.com/q_i_f_t/giftopia.html",
           "PyQT interface.  Only a few screenshots are available, but it looks promising.") */
);

?>
<table border=0 cellspacing=0 cellpadding=5>
<?php

foreach ($clients as $client => $info)
{
	echo " <tr><td valign=top>";

	if ($info[0] != "")
		echo "<a href=\"$info[0]\">";

	echo "$client";

	if ($info[0] != "")
		echo "</a>";

	echo "</td><td>$info[1]</td></tr>\n";
}

?>
</table>
<?php

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
