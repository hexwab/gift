#!/usr/bin/perl
#
# Convert a giFT-FastTrack nodes file to a KaZupernodes .kzn file.
#
#
while(<>)
{
    if (m/^([0-9\.]+)\s+([0-9]+)\s+/)
    {
        print "IP:$1|port:$2|user:|country:|state:|city:\n";
    }
}

