<%perl>
###############################################################################
##
## $Id: get-projects.m,v 1.3 2004/08/28 02:54:10 jasta Exp $
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

<%args>
	$sect
</%args>

<%once>
	use Page::Software;
</%once>

<%perl>
	my $page = Page::Software->new;
	my $projects = $page->get_project_tree;

	return $projects->findNode ($sect);
</%perl>
