<?php

$redirect = array ('/install.html' => 'http://gift.sourceforge.net/docs/?document=install.html',
                   '/faq.html'     => 'http://gift.sourceforge.net/docs/?document=faq.html',
                   '/gui-dev.html' => 'http://gift.sourceforge.net/docs/?document=interface.html');

foreach ($redirect as $from => $to)
{
	if ($REQUEST_URI == $from)
	{
		header("HTTP/1.1 301 Moved Permanently");
		header("Location: $to");
		return;
	}
}

?>
<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML 2.0//EN">
<HTML><HEAD>
<TITLE>404 Not Found</TITLE>
</HEAD><BODY>
<H1>Not Found</H1>
The requested URL <?php echo $REQUEST_URI; ?> was not found on this server.<P>
<HR>
<?php echo $SERVER_SIGNATURE; ?></BODY></HTML>
