#!/bin/sh

if [ "$1" ]; then
	user=$1
else
	user=$USER
fi

host='shell.sf.net'
group_path='/home/groups/g/gi/gift'
htdocs_path="$group_path/htdocs"

# sync
rsync -rltvz -C --delete -e ssh ./ $user@$host:$htdocs_path

# set permissions
ssh $user@$host "$group_path/set_perm.sh $user $htdocs_path"
