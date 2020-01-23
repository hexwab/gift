#!/usr/bin/perl -w
@a=split/\n/, `cat mini2.c`;

s/^.*\/\///g for @a;

for (140..$#a) {
    $temp=$a[$_];
    next unless $temp=~/pad|seed/;
    $a[$_]='// '.$a[$_];
    open(F, '>mini2temp.c') or die;
    print F join"\n",@a;
    close F;
    $r=system("tcc mini2temp.c test2.o .libs/fst_crypt.o -I.. .libs/enc_type_*.o ../.libs/md5.o -o test2 && ./test2");
    if ($r) {
	$a[$_]=$temp;
    } else {
	print "$_ OK\n";
    }
}
