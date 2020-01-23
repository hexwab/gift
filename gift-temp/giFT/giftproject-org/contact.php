<?php

///////////////////////////////////////////////////////////////////////////////
// TODO: This code could very much so benefit from the new clients.php
// rewrite using a database backend.  Hopefully I get some free time to
// really clean this up.
///////////////////////////////////////////////////////////////////////////////

require ('site.php');

layout_header ("CONTACT", "Contact");

///////////////////////////////////////////////////////////////////////////////

?>

<?php layout_section ("communication.c"); ?>
The giFT project has a somewhat dictatorial IRC channel that is reserved for
communication regarding the project's development only.  It seems to be <!-- ' -->
plagued by regular users who assume it is a support channel, which (as you
surely can understand) gets old fast.  As a result, we have setup this page
so that users and developers alike may choose a more appropriate method of
communication so as to not anger the giFT demigods.  As a general rule of
thumb you should assume that the mailing lists are preferred.

<dl>

 <dt><h2 class="bullet">Mailing Lists</h2></dt>
 <dd>
  <table border="0" width="100%">
<?php
contact_entry ("gift-users", ml_link ("gift-users"),
               "Discussion forum for regular giFT users (simple bug " .
               "reports are acceptable here).  Generally speaking, if " .
               "you don't know C, aren't planning on making [significant] " .
               "contributions to the project, or do not know how to use " .
               "gdb (or some other capable debugger), you should post " .
               "here.  We will attempt to staff some user-friendly admins " .
			   "willing to help you through various basic giFT problems.");
contact_entry ("gift-devel", ml_link ("gift-devel"),
			   "This list is reserved for complex technical discussions " .
			   "specifically about giFT (as opposed to OpenFT).  This list " .
			   "also serves as the notification address for tracker " .
			   "changes on the sourceforge.net project page and as such " .
			   "can be considered moderately high traffic.");
contact_entry ("gift-openft", ml_link ("gift-openft"),
			   "Similar to gift-devel, except that discussion here is " .
			   "related specifically to the OpenFT protocol.");
?>
  </table>
 </dd>

 <dt><h2 class="bullet">Sourceforge.net Modules</h2></dt>
 <dd>
  <table border="0" width="100%">
<?php
contact_entry ("Bug Tracker",
			   "http://sourceforge.net/tracker/?group_id=34618&atid=411725",
			   "Bug management/tracking system provided by sourceforge.net. " .
			   "You should note that this is not directly read by the core " .
			   "giFT developers, it is instead filtered by some of the " .
			   "less programmatic-inclined staffers.  More complex bugs " .
			   "may take a while longer to get noticed by the people who " .
			   "can actually fix them.");
contact_entry ("Patches",
			   "http://sourceforge.net/tracker/?group_id=34618&atid=411727",
			   "Similar to the bug tracker, but specifically designed to " .
			   "manage user-supplied patches.  Make sure you read " .
			   "README.dev (can be found in CVS) before submitting patches.");
contact_entry ("Public Forum",
			   "http://sourceforge.net/forum/forum.php?forum_id=108384",
			   "Open discussion forum for the giFT project.  This is very " .
			   "rarely frequented by the core developers as most of the " .
			   "posts here appear to be quite miscellaneous.  If none of " .
			   "the other contact methods seem appropriate, try this one " .
			   "but keep in mind that your post may go unanswered for very " .
			   "long periods of time");
?>
  </table>
 </dd>

 <dt><h2 class="bullet">Direct Contact</h2></dt>
 <dd>
  <table border="0" width="100%">
<?php
contact_entry ("Developers",
			   "http://sourceforge.net/project/memberlist.php?group_id=34618",
			   "Each individual developer is listed here and their direct " .
			   "email contact information.  You should probably avoid " .
			   "using these if any of the above contact channels seem more " .
			   "appropriate.");
contact_entry ("IRC", "http://www.freenode.net",
			   "Last but not least is our somewhat private IRC channel " .
			   "(irc.freenode.net:#gift). " .
			   "Please please please please read the above methods before " .
			   "contacting us here.  This channel is ONLY used for direct " .
			   "development-oriented communication with the core developers " .
			   "and possibly some of the other staff members.  If you are " .
			   "interested in helping with development, just want to idly " .
			   "listen to jasta's insane ramblings, or you have some " .
			   "topic of conversation that is much easier addressed in " .
			   "real-time, you should use this channel.  Please, <em>no</em> " .
			   "basic support questions.");
?>
  </table>
 </dd>

</dl>

<?php

///////////////////////////////////////////////////////////////////////////////

function ml_link ($ml)
{
	return "http://lists.sourceforge.net/lists/listinfo/$ml";
}

function contact_entry ($name, $link, $desc)
{
?>
  <!-- begin <?php echo $name; ?> -->
  <tr>
   <td width="20%" valign="top">
    <li><a href="<?php echo $link; ?>"><?php echo $name; ?></a>
   </td>
   <td>
	<?php echo $desc; ?>
   </td>
  </tr>
  <!-- end <?php echo $name; ?> -->
<?php
}

///////////////////////////////////////////////////////////////////////////////

layout_footer ($page, $page_desc);

?>
