#!/bin/sh
###############################################################################
## simple utility that can be used to generate a ~/.giFT/shares file
###############################################################################

file=$1

if [ ! -r "$file" ]; then
 exit 1
fi

stat_line=`stat -t "$file"`
mtime=`echo $stat_line | cut -d" " -f13`
size=`echo $stat_line | cut -d" " -f2`

hash=`md5sum "$file" | cut -d" " -f1`

echo "$mtime 1 $hash $size $file"
