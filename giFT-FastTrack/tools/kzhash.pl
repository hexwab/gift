#!/usr/bin/perl -wl
#
# Calculate the last 16 bytes of kazaa's new 'kzhash'.
# By Tom Hargreaves <hex@freezone.co.uk>, public domain.
#
use Digest::MD5 qw[md5];
my @h;
local $/=\32768;

# read and hash the input file in 32k blocks
while(<>) {
    push @h,md5($_);
}

# recursively hash pairs of hashes until there's only one left
do {
    my @k;
    my $a=0;
    do {
	no warnings;
	push @k,md5(@h[$a..$a+1]);
#	printf "%d %s\n",$a, md5_hex(@h[$a..$a+1]);
	$a+=2;
    } while ($a<@h);
    @h=@k;
#   print "h=".scalar @h;
} while (@h>1);

# print the hex output
my $out;
$out.=sprintf"%02x",ord for split'',$h[0];
print $out;
