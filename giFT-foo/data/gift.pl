# $Id: gift.pl,v 1.11 2002/10/19 05:27:41 jasta Exp $

use strict;
$|++;

my $script  = "sample";
my $version = "0.10.0";

print "registering script: $script (version: $version)...\n";
giFT::register ($script, $version, "goodbye", "");

giFT::add_hook ("upload_auth", "bar");

sub bypass_auth
{
	my ($user, $path, $mime, $size) = @_;

	if ($user =~ /^.*?\@((\d{1,3}.?){4})/) { $user = $1; }

	return 1 if ($user =~ /^(192|127)\./);

	if ($mime =~ /^image/ || $mime =~ /^text/)
	{
		return 1 if ($size < (500 * 1024));
	}

	return 0;
}

sub bar
{
	my ($user, $path, $mime, $size) = @_;

	my $ret = bypass_auth (@_) || -1;

	if ($ret == 1)
	{
		print "Bypassing share restriction for $user: $path ($size)\n";
	}

	return $ret;
}

sub goodbye
{
	print "I'll miss you!\n";
}
