#!/usr/bin/env perl
###############################################################################
##
## $Id: spider.pl,v 1.24 2004/04/12 07:17:18 jasta Exp $
##
## Copyright (C) 2003 giFT project (gift.sourceforge.net)
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

use Data::Dumper;
use Getopt::Long;

use LWP::UserAgent;

###############################################################################

my $opts =
{
	'help'         => 0,
	'input-nodes'  => undef,
	'output-nodes' => undef,
	'retry'        => 3
};

# honestly, duplicating these was just easier than not duplicating them...
my %opts_getopt =
(
	'h|help'           => \$opts->{'help'},
	'i|input-nodes=s'  => \$opts->{'input-nodes'},
	'o|output-nodes=s' => \$opts->{'output-nodes'},
	'r|retry=i'        => \$opts->{'retry'}
);

# simple cache to prevent processing the same node twice
my $nodes = {};

# file handle used when --output-nodes is specified
my $nodes_out;

###############################################################################

sub main
{
	my $conns = 0;
	my $handle;

	# get and provide basic handling of --help and such (might exit).
	opts_handle();

	# see if we should switch to reading from the nodes file
	if ((my $infile = $opts->{'input-nodes'}))
	{
		open ($handle, $infile) or
		  die "Cannot open $infile: $!";
	}
	else
	{
		# hmm, are we supposed to use *STDIN instead?
		$handle = 'STDIN';
	}

	# see if we should write a nodes file
	if ((my $outfile = $opts->{'output-nodes'}))
	{
		open ($nodes_out, ">$outfile") or
		  die "Cannot open $outfile: $!";
	}

	# access the next node either from the nodes file specified on the
	# command line or from stdin
	while (<$handle>)
	{
		my $nobj = parse_input_node_line ($_);
		next unless $nobj->{'klass'} & 0x2;

		$conns += crawl_search_node ($nobj);
	}

	# make sure to close the file, if not STDIN
	close $handle if (defined ($opts->{'nodes'}));
	close $nodes_out if defined $nodes_out;

	printf STDERR ("Spider finished.  %d node(s) contacted.\n", $conns);
}

sub usage
{
	my $argv0 = shift;

	print <<"EOF";

$argv0 [options]

  -h, --help                 Show this help message
  -i, --input-nodes <FILE>   Read nodes from <FILE> instead of STDIN
  -o, --output-nodes <FILE>  Output nodes to <FILE>
  -r, --retry <n>            Retry node connections <n> times before giving up

EOF
}

###############################################################################

sub opts_handle
{
	my $ret = GetOptions (%opts_getopt);

	if ($ret == 0 || $opts->{'help'})
	{
		usage ($0);
		exit;
	}
}

###############################################################################

sub parse_input_node_line
{
	my $line = shift || return undef;

	chomp $line;
	my ($timestamp, $uptime, $host, $port, $http_port, $klass, $version) =
	  split /\s+/, $line;

	return undef unless defined $host;
	return undef unless $klass >= 1;
	return undef unless hex ($version) > 0;
	return undef unless ($port > 0 && $http_port > 0);

	my $nobj =
	{
		'timestamp' => $timestamp,
		'uptime'    => $uptime,
		'host'      => $host,
		'port'      => $port,
		'http_port' => $http_port,
		'klass'     => $klass,
		'version'   => $version
	};

	return $nobj;
}

sub parse_http_node_line
{
	my $line = shift || return undef;

	chomp $line;
	my ($host, $port, $http_port, $klass, $version) =
	  split /\s+/, $line;

	return undef unless defined $host;
	return undef unless $klass >= 1;
	return undef unless hex ($version) > 0;

	# make sure http port reflects the firewalled status, as it once did
	$port == 0 and
	  $http_port = 0;

	my $nobj =
	{
		'host'      => $host,
		'port'      => $port,
		'http_port' => $http_port,
		'klass'     => $klass,
		'version'   => hex ($version)
	};

	return $nobj;
}

###############################################################################

sub crawl_search_node
{
	my $nobj = shift || die;
	my $ret = 0;

	# we've already seen this node, ignore it...
	return 0 if exists $nodes->{$nobj->{'host'}};
	$nodes->{$nobj->{'host'}} = {};

	printf STDERR ("Crawling %s:%hu...", @$nobj{qw/host http_port/});

	# access the complete unfiltered list of nodes connected
	my @nlist = get_node_list ($nobj);

	unless (@nlist)
	{
		printf STDERR "Failed\n";
		return 0;
	}

	if (defined $nodes_out)
	  { printf $nodes_out ("%s\n", fmt_output_file ($nobj)); }

	# break up the nodelist so that it may be stored for map production
	my @nlist_search = ();
	my @nlist_children = ();

	foreach my $nobj (@nlist)
	{
		push @nlist_search, $nobj   if $nobj->{klass} & 0x2;
		push @nlist_children, $nobj if $nobj->{klass} & 0x100;
	}

	# dump the information that we care about for the map
	dump_node ($nobj, \@nlist_search, \@nlist_children);

	printf STDERR "Done\n";

	# this loop exists after dump_node so that we can dump each nodes spider
	# information before we move on to another node
	foreach my $nobj (@nlist_search)
	{
		# recursively spider to all search peers from this node
		$ret += crawl_search_node ($nobj);
	}

	return $ret + 1;
}

sub get_node_list
{
	my $nobj = shift || die;

	# initialize the nodelist for some reason :)
	my @nlist = ();

	my $ua = LWP::UserAgent->new ('timeout' => 300);
	die unless defined $ua;

	# lazily access the entire nodes file from this remote peer
	my $url = sprintf ("http://%s:%hu/nodes", @$nobj{qw/host http_port/});

	my $req = HTTP::Request->new ('GET' => $url);
	die unless defined $req;

	my $resp;

	# if retry is 0, we still want to try once, right? :)
	my $tries = $opts->{'retry'} + 1;

	while (--$tries > 0)
	{
		$resp = $ua->request ($req);
		last if $resp->is_success;

		printf STDERR ("Failed\n");
		printf STDERR ("Retrying %s:%hu...", @$nobj{qw/host http_port/});
	}

	return @nlist unless defined $resp && $resp->is_success;
	my $nodes = $resp->content;

	# line-by-line reading is easier
	my @lines = split (/\n/, $nodes);

	foreach my $line (@lines)
	{
		my $nobj = parse_http_node_line ($line);
		next unless defined $nobj;

		push @nlist, $nobj;
	}

	return @nlist;
}

###############################################################################

sub dump_node
{
	my ($nobj, $peers, $children) = @_;

	# dont consider nodes that dont have any network connectivity
	return unless (scalar @$peers + scalar @$children > 0);

	printf STDOUT ("%s", fmt_node ($nobj));

	dump_nodelist (@$peers);
	dump_nodelist (@$children);

	print STDOUT "\n";
}

sub dump_nodelist
{
	# hackish logic to avoid leaving trailing commas
	print STDOUT ",";

	foreach my $nobj (@_)
	{
		# more hackish logic to avoid trailing spaces
		printf STDOUT (" %s", fmt_node ($nobj));
	}
}

sub fmt_node
{
	my $nobj = shift;

	# implicitly return the formatted buffer
	sprintf ("%s:%hu(0x%04x:0x%08x)",
	         @$nobj{qw/host http_port klass version/});
}

sub fmt_output_file
{
	my $nobj = shift;
	my ($timestamp, $uptime);

	my $timestamp = $nobj->{'timestamp'} || time;
	my $uptime    = $nobj->{'uptime'}    || 1;
	my $class     = $nobj->{'klass'} & 0x7;

	# implicitly return the formatted buffer
	sprintf ("%d %d %s %hu %hu %d %d",
	         $timestamp, $uptime,
	         @$nobj{qw/host port http_port/}, $class, $nobj->{version});
}

###############################################################################

main();
