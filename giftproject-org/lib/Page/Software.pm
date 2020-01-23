###############################################################################
##
## $Id: Software.pm,v 1.2 2004/08/24 16:06:34 jasta Exp $
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

package Page::Software;

###############################################################################

use strict;

use Tree::Simple;
use Site::DBH;

###############################################################################

my $stmts =
{
	'get-types' => q
	{
		SELECT
			id, parentId, name
		FROM
			softwareType
	},

	'get-projects' => q
	{
		SELECT
			id, name, url, description
		FROM
			project
		WHERE
			softwareTypeId = ?
	},

	'get-screenshots' => q
	{
		SELECT
			url, urlThumb, description
		FROM
			projectScreenshot
		WHERE
			projectId = ?
	},
};

###############################################################################

sub new
{
	my $self = bless ({}, shift);
	$self->_populate;

	return $self;
}

###############################################################################

sub _populate
{
	my $self = shift;
	my $dbh = Site::DBH->new ($stmts);

	my $root = Tree::Simple->new (undef, Tree::Simple->ROOT);
	my %byid = ();
	my %byname = ();

	$stmts->{'get-types'}->execute;

	while (my ($id, $parentId, $name) = $stmts->{'get-types'}->fetchrow_array)
	{
		my $node = Tree::Simple->new ($name);
		$byid{$id} = $node;
		$byname{$name} = $node;

		my $parent = $byid{$parentId} || $root;
		$parent->addChild ($node);

		$stmts->{'get-projects'}->execute ($id);

		while ((my $pdata = $stmts->{'get-projects'}->fetchrow_hashref))
		{
			$stmts->{'get-screenshots'}->execute (delete $pdata->{'id'});

			$pdata->{'screenshot'} = 
				$stmts->{'get-screenshots'}->fetchrow_hashref;
			
			$node->addChild (Tree::Simple->new ($pdata)); 
		}
	}

	$self->{'root'} = $root;
}

###############################################################################

sub get_project_tree
{
	return (shift)->{'root'};
}

sub Tree::Simple::findNode
{
	my $root = shift;
	my $node = shift;
	
	foreach ($root->getAllChildren())
	{
		my $childval = $_->getNodeValue;

		if (ref $childval)
		{
			ref $childval eq 'HASH' and
				return $_ if (%$node == %$childval);
		}
		else
		{
			# Me, it's probably a string.
			return $_ if ($node eq $childval);
		}
	}

	return undef;
}

###############################################################################

1;
