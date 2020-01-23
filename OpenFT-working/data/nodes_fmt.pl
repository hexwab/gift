#!/usr/bin/env perl
###############################################################################
##
## $Id: nodes_fmt.pl,v 1.9 2003/12/26 05:34:59 jasta Exp $
##
## Simple script designed to tidy up an active nodes file for standard
## distribution.
##
## Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
##
## This program is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by the
## Free Software Foundation; either version 2, or (at your option) any
## later version.
##
## This program is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
###############################################################################

use strict;

###############################################################################

my $version_curr = 0x00020102;
my $nodes =
{
	# hardcode a central node to ease testing
	'68.116.100.143' =>
	{
		'vitality'  => 1500000000,
		'uptime'    => 15000000,
		'port'      => 1215,
		'http_port' => 1216,
		'class'     => 0x3,
		'version'   => $version_curr
	}
};

###############################################################################

# parse in all the nodes from stdin or the argument params
while (<>)
{
	my ($vitality, $uptime, $host, $port, $http_port, $klass, $version) = split;

	next unless $vitality                > 0;
	next unless $uptime                  > 0;
	next unless $host                    > 0;
	next unless $port                    > 0;
	next unless $http_port               > 0;
	next unless $klass                   & (0x2 | 0x4);
	next unless ($version & 0xffffff00) == ($version_curr & 0xffffff00);

	next if exists $nodes->{$host};
	$nodes->{$host} =
	{
		'vitality'  => $vitality,
		'uptime'    => $uptime,
		'host'      => $host,
		'port'      => $port,
		'http_port' => $http_port,
		'class'     => $klass,
		'version'   => $version
	};
}

# construct a list of sorted hosts as OpenFT would do it for itself
my @sorted = sort
  { $nodes->{$b}->{'version'}  <=> $nodes->{$a}->{'version'} ||
    $nodes->{$b}->{'vitality'} <=> $nodes->{$a}->{'vitality'}
  } keys %$nodes;

# iterate the sorted list and print to stdout
foreach my $node (@sorted)
{
	my $noderef = $nodes->{$node};

	printf ("%d %d %s %d %d %d %d\n",
			$noderef->{vitality}, $noderef->{uptime},
			$node, $noderef->{port}, $noderef->{http_port},
			$noderef->{class}, $noderef->{version});
}
