#!/usr/bin/perl
#
# Convert a KaZupernodes .kzn file to giFT-FastTrack nodes file.
#
#

# klasses:
#  0: user node
#  1: supernode
#  2: index node

$klass = 2;
$load = 50;
$last_seen = 0;

while(<>)
{
    if (m/^IP:([0-9\.]+)\|port:([0-9]+)\|/)
    {
	    print "$1 $2 $klass $load $last_seen\n";
    }
}

