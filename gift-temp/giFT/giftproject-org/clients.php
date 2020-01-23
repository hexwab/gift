<?php

///////////////////////////////////////////////////////////////////////////////

require ('site.php');

layout_header ("CLIENTS", "Clients");

///////////////////////////////////////////////////////////////////////////////

?>

<?php layout_section ("clients/"); ?>

<p>
 "Client", "front end", "interface" and "GUI" are interchangably used, and
 all mean the same thing. Namely a program that communicates with giFT's <!-- ' -->
 interface protocol and is used to control the daemon.  Please note that
 only giFTcurs will be supported, and only if you keep it updated through
 CVS as things are far too unstable to predict whether or not giFTcurs
 releases and giFT CVS will properly communicate.
</p>

<dl>

<?php

$q = <<<SQL
	SELECT
		id, name
	FROM
		clientsgroup
SQL;

if (!($result = mysql_query ($q)))
	site_err_mysql ($q);
else
{
	while (($row = mysql_fetch_assoc ($result)))
		add_section ($row[id], $row[name]);
}

function add_section ($id, $group)
{
?>
 <dt><h2 class="bullet"><?= $group; ?> Clients</h2></dt>
 <dd>
  <table border="0" width="100%">
<?php

	$q = <<<SQL
		SELECT
			name, url, descr
		FROM
			clients
		WHERE
			clientsgroup_id = $id
		ORDER BY
			name
SQL;

	if (!($result = mysql_query ($q)))
		site_err_mysql ($q);
	else
	{
		while (($row = mysql_fetch_assoc ($result)))
			add_client ($row['name'], $row['url'], $row['descr']);
	}

?>
  </table>
 </dd>

<?php
}

function add_client ($client, $link, $desc)
{
	if ($link)
		$client = "<a href=\"$link\">$client</a>";

?>
   <tr>
    <td width="20%" valign="top">
     <li><?php echo $client; ?>
    </td>
    <td>
     <?php echo $desc; ?>
    </td>
   </tr>
<?php
}

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
