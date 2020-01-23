<%perl>
###############################################################################
##
## $Id: show-project.m,v 1.4 2004/08/28 02:54:10 jasta Exp $
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
	$project
</%args>

<li>
	<dt>
	
%	if (defined ($project->{'url'}))
%	{
		<a href="<% $project->{'url'} %>">
%	}

			<% $project->{'name'} %>

%	if (defined ($project->{'url'}))
%	{
		</a>
%	}
	
	</dt>

% if (defined ((my $sshot = $project->{'screenshot'})))
% {

	<dd class="screenshot">

		<a href="<% $sshot->{'url'} %>">
			<img src="<% $sshot->{'urlThumb'} %>"
			     alt="Screenshot: <% $sshot->{'description'} %>" />
		</a>

	</dd>

% }

	<dd>
		<% $project->{'description'} %>
	</dd>

	<dd style="clear: both" />
</li>
