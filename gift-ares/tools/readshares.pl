#!/usr/bin/perl -lw

#
# $Id: readshares.pl,v 1.2 2005/12/17 23:09:58 mkern Exp $
#
# Copyright (C) 2005 giFT-Ares project
# http://developer.berlios.de/projects/gift-ares
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any
# later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#

undef $/;
$_=<>;
my $files=0;
while ($_) {
my $len=unpack'v',substr $_,0,2,'';
my $rec=substr $_,0,$len,'';
my $zero=unpack'C',substr $_,0,1,'';
print "reclen=$len\n";
die if $zero;

my ($unk1, $tokenslen)=unpack'Cv', substr $rec,0,3,'';
printf "unk1=0x%x, tokenslen=%d\n", $unk1, $tokenslen;
die if $unk1!=0x1c;
my $tokens=substr $rec,0,$tokenslen,'';
while ($tokens) {
    my ($unk, $tok, $tlen)=unpack'CvC', substr $tokens,0,4,'';
    my $token=substr $tokens,0,$tlen,'';
    printf "unk=%d, tokenized=%02x, token='%s' (len %d)\n", $unk, $tok, $token,$tlen;
}
my ($bitrate,$freq,$dur)=unpack"VVV",substr $rec,0,12,'';
printf "bitrate=%d freq=%d dur=%d\n", $bitrate,$freq,$dur
    if $bitrate || $freq || $dur;

my ($realm,$filesize)=unpack"CV", substr $rec,0,5,'';
print "realm: ".({1=>'audio',5=>'video',6=>'document',7=>'image'}->{$realm}||'???');
my ($hash)=substr $rec,0,20,'';
print "hash: ".(join '', map {sprintf "%02x",$_} unpack "C*", $hash);
my ($ext)=unpack "Z*",$rec; substr $rec,0,1+length $ext,'';
die if $ext!~/^\./;
print "extension: $ext";

#print "left=".length $rec;
while ($rec) {
    my $type=unpack"C",substr $rec,0,1,'';
    if ($type==1 || $type==2 || $type==3 || $type==6 || $type==7 || $type==16) {
	my $meta=unpack "Z*",$rec; substr $rec,0,1+length $meta,'';
	print {1=>'title',2=>'artist',3=>'album',6=>'year',7=>'codec',16=>'filename'}->{$type}
	.': '.$meta;
    } elsif ($type==4) {
	if ($realm==1) { #audio
	    my ($bitrate,$dur)=unpack"vV",substr $rec,0,6,'';
	    print "bitrate: $bitrate dur: $dur";
	} elsif ($realm==7) { #image
	    my ($w, $h, $unk)=unpack"vvV",substr $rec,0,8,'';
	    print "width: $w height: $h depth: $unk";
	} elsif ($realm==5) { #video
	    my ($w, $h, $bitrate)=unpack"vvV",substr $rec,0,8,'';
	    print "width: $w height: $h rate: $bitrate";
	} elsif ($realm==6) { #document
	} else {
	    die
	}
    } else {
	die "unknown meta type $type";
    }
}
$files++;
}
print "$files files";
