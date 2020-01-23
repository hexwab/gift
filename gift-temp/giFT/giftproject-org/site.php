<?php

///////////////////////////////////////////////////////////////////////////////
// MySQL Connections

$dbinfo = slurp_file ('dbinfo');

// we are doing this funky logic here because if we explode first, we cant
// test for an empty value as reliably
if ($dbinfo)
	$dbinfo = explode ("\n", $dbinfo);
else
	$dbinfo = array ('localhost', 'root', '', 'gift');

list ($dbhost, $dbuser, $dbpass, $dbname) = $dbinfo;
init_mysql ($dbhost, $dbuser, $dbpass, $dbname);

///////////////////////////////////////////////////////////////////////////////
// HTML Layout

$site_http = '';
require_once ('lib/layout.php');

///////////////////////////////////////////////////////////////////////////////
// Misc Functions (that belong elsewhere)

function slurp_file ($file)
{
	if (!($array = @file ($file)))
		return '';

	return implode ("", $array);
}

function site_err ($err)
{
	printf ("<strong>Error:</strong> %s\n", $err);
}

function site_err_mysql ($q, $err = null)
{
	if ($err !== null)
		$str = "$err: ";

	$str .= sprintf ("<em>%s</em>: %s", $q, mysql_error ());
	site_err ($str);
}

function init_mysql ($dbhost, $dbuser, $dbpass, $dbname)
{
	if (!mysql_pconnect ($dbhost, $dbuser, $dbpass))
	{
		echo "Unable to connect to $dbhost\n";
		return;
	}

	if (!mysql_select_db ($dbname))
	{
		echo "Unable to switch to database $dbname\n";
		return;
	}
}

///////////////////////////////////////////////////////////////////////////////

?>
