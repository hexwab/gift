#!/usr/bin/env perl
###############################################################################
##
## $Id: spider.pl,v 1.5 2003/05/17 05:23:30 jasta Exp $
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

use Openft;

@Openft::version=(0,1,2,3);

###############################################################################

for (@ARGV) {
    spider_to ($_);
}

sub spider_to
{
	my $node = shift;
	my $ret = 0;

	# establish a new connection to spider
	my $c = Openft->new ($node);

	printf STDERR "Crawling $node...";

	$c->connect ||
	  (warn ("error: " . $c->get_err() . "\n"), return 0);

	printf STDERR "Done\n";

	$c->disconnect;
}
