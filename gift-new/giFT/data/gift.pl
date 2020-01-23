# $Id: gift.pl,v 1.8 2002/06/20 00:36:31 jasta Exp $

use strict;
$|++;

my $script  = "jasta owns you";
my $version = "0.10.0";

print "registering script: $script (version: $version)...\n";
giFT::register ($script, $version, "goodbye", "");

giFT::add_hook ("download_complete", "foo");
giFT::add_hook ("upload_auth",       "bar");

sub foo
{
	my ($path, $md5, $state_path) = @_;
	
	$path =~ s/\"/\\"/;
	my $sum = `md5sum "$path"`;
	
	$sum =~ s/\s.*$//s;
	
	if ($md5 ne $sum)
	{
		print "corrupted file detected!!!!!!!!\n";
	}
	
	print "finished downloading: $path\n";
}

sub bar
{
	my ($path, $cur, $ttl) = @_;
	
	my ($ext) = $path =~ /\.(.*?)$/;
	
	return 1 if ($ext eq "jpg" || $ext eq "png" || $ext eq "gif");

	print "default behaviour applied for $path ($cur, $ttl)\n";
	
	return -1;
}

sub goodbye
{
	print "I'll miss you!\n";
}
