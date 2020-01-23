<?php

$section   = "DOWNLOAD";
$page      = "cvs.php";
$page_desc = "Download";

require ("../layout.php");

layout_header ($section, $page, $page_desc);

///////////////////////////////////////////////////////////////////////////////

?>

<?php layout_section ("CVS/"); ?>
Currently CVS is the most reliable way (sad, isnt it?) to download giFT, as
0.9.7 is horribly out of date and doesn't apply to this page in any way. Please
read the <a href="../docs/?document=install.html">Installation
Guide</a> for complete instructions. If you have problems installing
giFT, don't come to us until you've read the complete <a
href="../docs/?document=install.html">Installation Guide</a>. At least three
times.<br><br>
<h2>Short installation instructions:</h2>
<tt>
cvs -d':pserver:anonymous@cvs.gift.sf.net:/cvsroot/gift' login<br>
</tt>
(Just press enter when prompted for a password.)<br>
<tt>
cvs -z3 -d':pserver:anonymous@cvs.gift.sf.net:/cvsroot/gift' co giFT<br>
cd giFT<br>
./autogen.sh<br>
make<br>
su -c 'make install'<br>
giFT-setup<br>
giFT<br>
</tt>
<br>
Update your CVS copy with:<br><br>
<tt>
cd giFT<br>
cvs update<br>
</tt>

<?php

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
