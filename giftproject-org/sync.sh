#!/bin/sh
###############################################################################
##
## $Id: sync.sh,v 1.3 2004/08/28 02:53:55 jasta Exp $
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

rsync -ravL --delete -C --progress \
	--exclude "cgi-bin" \
	--exclude "DBInfo.pm" \
	--exclude "*.sh" \
	./ shell.sourceforge.net:/home/groups/g/gi/gift/htdocs/
