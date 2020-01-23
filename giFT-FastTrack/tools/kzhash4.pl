#!/usr/bin/perl -wl
#
# Calculate the last 16 bytes of kazaa's new 'kzhash'.
# By Tom Hargreaves <hex@freezone.co.uk>, public domain.
# Take 3: low memory version, no virtual blocks
#
use Digest::MD5 qw[md5];
my @h;
local $/=\32768;

my $block=0;

sub hashme {
    my $b=shift;
    while (!($b &1)) {
	$b>>=1;
	my @a;
	unshift @a, pop @h;
	unshift @a, pop @h;
	push @h, md5(@a);
    }
}

# read and hash the input file in 32k blocks
while(<>) {
    push @h,md5($_);
    hashme(++$block);
}

if (!$block) {
# kludge #1: zero input
    push @h,md5('');
} elsif ($block==1) {
# kludge #2: <32K input
    push @h, md5 pop @h;
}

# clean up any remaining odd blocks

# kludge #3: avoid infinite loop
if ($block) {
    $block>>=1 while (!($block &1));
    $block &=~1;
}

while ($block) {
    my @a;
    my $pair=($block & 1);
    $block>>=1;

    unshift @a, pop @h;
    unshift @a, pop @h if $pair;
    push @h, md5(@a);
}

die "oops" if $#h;

# print the hex output
my $out;
$out.=sprintf"%02x",ord for split'',$h[0];
print $out;
