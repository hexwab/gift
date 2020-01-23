#!/usr/bin/perl -w

#
# $Id: gift2shares.pl,v 1.3 2005/12/17 23:09:58 mkern Exp $
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

use MIME::Base64;

sub rint { unpack'L',substr $_[0],0,4,'' }
sub strnul { my ($str)=unpack "Z*",$_[0]; substr $_[0],0,1+length $str,''; $str }

local $/=undef;
$_=<>;
REC:
while ($_) {
    my $len=rint $_;
    my $rec=substr $_,0,$len,'';
    
    my $mtime=rint $rec;
    my $size=rint $rec;
    my (%hash,%meta);
    my $mime=strnul $rec;
    my $root=strnul $rec;
    my $path=strnul $rec;
    next REC if $path=~/\"|\n/s;
    
    while (my $key=strnul $rec) {
	my $hlen=rint $rec;
	my $val=substr $rec,0,$hlen,'';
	die unless $key=~s/^H-//;
	$hash{$key}=$val;
    }
    next REC unless $hash{SHA1}; # should be unnecessary
    while (my $key=strnul $rec) {
	my $val=strnul $rec;
	next REC if ($key.$val)=~/\"|\n/;
	$val/=1000 if $key=~/^bitrate$/;
	$meta{$key}=$val;
    }
    die if $rec;
    printf "share \"%s\" %u %u %s %s\n", 
    $path, $size, 
    {audio=>1,video=>5,image=>7,text=>6,application=>3}->{(($mime=~m[(.*)/])[0])}||'0', 
    (map {$_?substr(encode_base64($_),0,28):'-'} $hash{SHA1}),
    join' ',map {/ /?"\"$_\"":$_} %meta;
}
