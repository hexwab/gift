#!/usr/bin/perl

# Small Gift interface example

use Gift::Interface;
use Gift::Config;

my $Interface = new Gift::Interface;
my $Config = new Gift::Config;

# Turn on debugging for download and search functions
# Enable all using "+all", disable specific with "-search"
$Interface->debugmessage ("+download", "+search");

# Don't really execute commands
# $Interface->pretend (1);

# Determine host and port of giFT daemon
my $host = $Config->read("ui/ui.conf", "daemon", "host");
my $port = $Config->read("ui/ui.conf", "daemon", "port");

# Connect to daemon
$Interface->connect ($host, $port);


# Perform search (realm, query, exclude, Protocol)
$Interface->search ("audio", "eminem", "dre", "OpenFT");

my %download;


# While Gift socket is open
while ($Interface->can_read)
{
	my ($head,%tree) = $Interface->read;

	# ITEM [id] user(...) file(...) [...] ;
	if ($head eq "ITEM")
	{
		# Parse out leading path (if any)
		my $file = $tree{'file'} =~ /^\/(.*)\/(.*)$/;
		($file = $2) ||= $tree{'file'};


		# Show some information about the item
		print STDERR "\n[\e[1;32m!\e[m]\t($tree{mime}) $file\n";
		print STDERR "\tUser: $tree{user}\n";
		print STDERR "\tHash: $tree{hash}\n\tSize: $tree{size} bytes\n";
		print STDERR "\tBitrate: $tree{'meta'}{'bitrate'}\n";


		# Add the data needed to start a download
		# Note: It is being overwritten each ITEM
		@download =
		(
			$tree{url},
			$tree{hash},
			$tree{size},
			$file,
			$tree{user}
		);
	}
}

# Okay, that was fun, download that file!
$Interface->download (@download);

# Shouldn't be attached at this point
# But just in case
$Interface->detach;
