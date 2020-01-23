<%perl>
###############################################################################
##
## $Id: get-news.m,v 1.6 2004/09/05 02:41:51 jasta Exp $
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
</%perl>

<%once>
	use XML::RSS;
</%once>

<%perl>

	my $rss = XML::RSS->new;

	# TODO: This is evil, and I should know better.
	open (RDF, '/home/groups/g/gi/gift/project-news.rdf') or
	  return undef;

	local $/;
	$rss->parse (<RDF>);

	close RDF;

	# Now mung it the parsed structure to fix sourceforge suckage.
	foreach my $item (@{$rss->{'items'}})
	{
		($item->{'sf:user'}) = $item->{'author'} =~
			m/^([^@]+)/ or next;

		$item->{'sf:userLink'} = 
			'http://www.sourceforge.net/users/' . $item->{'sf:user'};
	}

	return $rss->{'items'};

</%perl>
