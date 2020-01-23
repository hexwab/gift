#!/bin/sh
###############################################################################
# $Id: doxygen.sh,v 1.2 2003/06/30 09:15:21 jasta Exp $
###############################################################################

if [ "$1" ]; then
	user=$1
else
	user=$USER
fi

host='shell.sf.net'
group_path='/home/groups/g/gi/gift'
htdocs_path="$group_path/htdocs"

doxygen doxygen.conf

# sync
rsync -rltvz -C --delete -e ssh ./output/html $user@$host:$htdocs_path/doxygen

# set permissions
ssh $user@$host "$group_path/set_perm.sh $user $htdocs_path"
