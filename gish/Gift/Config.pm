#!/usr/bin/perl

package Gift::Config;

use strict;
use warnings;
#use Carp qw(cluck);
#use diagnostics;

our(@ISA, $VERSION);
@ISA = qw(Gift);
$VERSION = "1.4";

sub new { bless {}, shift }

sub read
{
	my $self = shift;

	my $file = $_[0];
	my $section = $_[1];
	my $option = $_[2];

	if ($option =~ /(.*?)\/(.*)/)
	{
		$section = $1;
		$option = $2;
	}

	open(CONF,"<$ENV{HOME}/.giFT/$file") or return 0;

	my $savesection;
	while (<CONF>)
	{
		next if (/^\s*#/);

		if (/^\[(.+)\]$/i)
		{
			$savesection = $1;
		}

		if (/^\Q$option\E\s*=\s*(.+)$/)
		{
			if ($savesection eq $section)
			{
				close CONF;
				return $1;
			}
		}
	}

	close CONF;
	return 0;
}

1;
