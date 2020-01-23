###############################################################################
##
## $Id: DBH.pm,v 1.2 2004/08/24 16:06:34 jasta Exp $
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

package Site::DBH;

###############################################################################

use strict;
use DBI;

use Site::DBInfo;

###############################################################################

sub new
{
	my $class = shift;
	my $stmts = shift;

	my $i = Site::DBInfo->new;

	my $dbh = DBI->connect ($i->{'dsn'}, $i->{'user'}, $i->{'pass'}) or
	  return undef;

	if (defined ($stmts))
	{
		foreach my $name (keys %$stmts)
		  { $stmts->{$name} = $dbh->prepare (delete $stmts->{$name}); }
	}

	return $dbh;
}

###############################################################################

1;
