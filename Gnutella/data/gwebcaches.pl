#! /usr/bin/perl
###############################################################################
# $Id: gwebcaches.pl,v 1.1 2004/07/29 12:54:21 mkern Exp $
###############################################################################
#
# A simple script which gets a list of active Gnutella webcaches from 
# http://gcachescan.jonatkins.com and prints them in gwebcaches format.
#
# Usage: ./gwebcaches.pl > gwebcaches
#
###############################################################################
use LWP::UserAgent;
use HTML::TableExtract;


# Get HTML page
###############################################################################

# Using this url the returned table will be sorted by Score
my $url = "http://gcachescan.jonatkins.com";

print STDERR "\nGetting webcaches from $url\n";

my $ua = LWP::UserAgent->new ();
die "Couldn't create LWP::UserAgent" unless defined $ua;

my $req = HTTP::Request->new ('GET' => $url);
die "Couldn't create HTTP::Request" unless defined $req;

my $resp = $ua->request ($req);
die "HTTP request failed" if (not defined $resp or not $resp->is_success);


# Parse HTML
###############################################################################

print STDERR "Parsing HTML\n";

my $te = new HTML::TableExtract( headers => [('Cache Url', 'Score', 'Ping')],
                                 keep_html => 1 );
die "Couldn't create HTML::TableExtract" unless defined $te;

$te->parse($resp->content);

# Examine first matching table
my $ts = $te->first_table_state_found();
die "Webcache table found in page" unless scalar %$ts;

foreach $row ($ts->rows)
{
	my $cache_url = @$row[0];
	my $cache_score = @$row[1];
	my $cache_ping = @$row[2];

	next if ($cache_url =~ /^\s*$/);
	next if ($cache_ping =~ /Failed/);

	# Remove details link
	$cache_url =~ s,^\s*<a.+>\s*\?\s*</a>\s*,,;

	# Get actual href
	$cache_url =~ s,^\s*<a.*\s+href="(.+)".*>.*,\1,;

	print "$cache_url 0\n";
}

print STDERR "Finished\n";

