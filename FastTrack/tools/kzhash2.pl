#!/usr/bin/perl -wl
#
# Calculate the last 16 bytes of kazaa's new 'kzhash'.
# By Tom Hargreaves <hex@freezone.co.uk>, public domain.
# Take 2: low memory version,
#
use Digest::MD5 qw[md5 md5_hex];
my @h;
local $/=\32768;

my $block=0;

sub hashme {
    my $b=shift;
    while (!($b &1)) {
	$b>>=1;
	print "hashing $block";
	my @a;
	unshift @a, pop @h;
	unshift @a, pop @h;
	if ($a[0] || $a[1]) { # ewwwww
	    push @h, md5(@a);
	} else {
	    push @h,'';
	}
    }
}

# read and hash the input file in 32k blocks
while(<>) {
    push @h,md5($_);
    hashme(++$block);
}

# clean up any remaining odd blocks
while ($#h) {
    push @h, '';
    hashme(++$block);
}

die "oops" if $#h;

# print the hex output
my $out;
$out.=sprintf"%02x",ord for split'',$h[0];
print $out;
