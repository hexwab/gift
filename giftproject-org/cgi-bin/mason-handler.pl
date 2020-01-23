#!/usr/bin/perl
###############################################################################
##
## $Id: mason-handler.pl,v 1.3 2004/08/28 02:53:55 jasta Exp $
##
## Copyright (C) 2004 giFT project (gift.sourceforge.net)
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

BEGIN
{
	#
	# To correct an extremely irritating bug in HTML::Mason related to
	# an assumption that Perl is newer than it happens to be on the
	# sourceforge.net web server.
	#
	sub File::Spec::rel2abs
	{
		return ((shift)->canonpath (shift));
	}
	
	# Site install on shell.sourceforge.net.
	unshift @INC, '/home/groups/g/gi/gift/perl/site/lib/perl5/site_perl/5.6.0';
	unshift @INC, '/home/groups/g/gi/gift/perl/site/lib/perl5/site_perl/5.8.0';
};

###############################################################################

use strict;
use HTML::Mason::CGIHandler;

###############################################################################

push @INC, '/var/www/dev/giftproject-org/lib';

###############################################################################

my $h = HTML::Mason::CGIHandler->new;
$h->handle_request;
