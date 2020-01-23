#!/usr/bin/perl -w
use strict;

use CGI qw[:standard start_ul end_ul start_table end_table escape unescape];

use giFT::Daemon;
use giFT::Shares;
use giFT::Stats;
use giFT::TransferHandler;

# the globally-unique hostname on which giFT is running (i.e. not "localhost")
my $hostname=$ENV{SERVER_NAME}; 

# your OpenFT HTTP port
my $port=1216; 

use vars qw[$daemon $shares $stats $th %tree];

use subs qw[myescape cb_shares];

if (!$daemon) {
    $daemon=new giFT::Daemon($hostname,undef,1,0);
    if (!$daemon) {
	print header,html("Unable to connect to giFT: $!");
	exit;
    }
    $daemon->attach;
    undef $_ for ($shares, $stats, $th);
}
$shares||=new giFT::Shares($daemon,0,undef,undef,undef,\&cb_shares);
$stats||=new giFT::Stats($daemon);
$th||=new giFT::TransferHandler($daemon);

$stats->pump;

do {
    $daemon->poll;
} while (!$shares->share_status);

my $path=unescape path_info;
my $dir=\%tree;
#$path=~s#/../[^/]*/#/#g;
$path=~s#(?=.)/$##;
my $valid=1;
my @path;
for (split /\//,$path) {
    next if !$_;
    $valid=0, last if !exists $dir->{$_};
    $dir=$dir->{$_};
    push @path, $_;
}
if (!$valid) {
    print header{status=>'404 Not Found'};
} else {
    print header;

    print start_html{title=>"giFT: ".($path?$path:'/'),style=>{-verbatim=><<'EOS'}};
body {background: #ddf; text-color: white}
#status {background: #aac;padding:.1em;border:2px solid black;text-align:center}
#files {background: #eef; border: 1px solid black;width:100%}
.size { text-align: right; padding-right: .5em}
.value {font-weight: bold}
.sep {color: #ddf}
.meta {font-style: italic;padding-right: .5em}
EOS

    if ($shares->sync_status) {
	print p "Synchronization in progress (".($shares->sync_status)." files scanned).";
    } 
    if ($shares->shares) { # not mutually exclusive with syncing
	print div{id=>'status'}, join span({class=>'sep'},' | '),map{s/(: .*)/span{class=>'value'},$1/e;$_}(
					     scalar localtime,
					     'Files: '.$shares->shares,
					     'Size: '.sprintf("%.2fGB",($stats->shared_stats)[1]/(1<<30)),
					     'Uploads: '.$th->uploads,
					     'Bandwidth: '.sprintf("%.2fkB/s",($th->bandwidth)[0]/1024),
					     "PID: $$",
					     );
#    print p 'Hidden: '.($shares->hidden_status?'yes':'no');
	my $linkpath;
	{
	    my $i=@path;
	    $linkpath=a{href=>(join'/',('..')x$i or'.').'/'},'/';
	    for (@path) {
		$linkpath.=a{href=>(join'/',('..')x--$i or'.').'/'},escapeHTML $_;
		$linkpath.='/';
	    }
	    chop $linkpath if @path;
	}
	print h2{id=>'dirheader'},"Directory listing of $linkpath";
	print start_ul{id=>'subdirs'};
	for (sort keys %$dir) {
	    next if !$_;
	    my $subdir=$dir->{$_};
	    my $files=($subdir->{''} && keys %{$subdir->{''}});
	    my $dirs=keys(%$subdir);
	    $dirs-- if $files;
	    my @info;
	    push @info, "$files file".($files==1?'':'s') if ($files);
	    push @info, "$dirs subdirector".($dirs==1?'y':'ies') if ($dirs);
	    
	    print li[a({href=>escape($_).'/'},escapeHTML $_). ' ['.(@info?join', ',@info:'empty').']'];
	}
	print end_ul;
	$path=~s/.//; # :(
	if ($dir->{''} && %{$dir->{''}}) {
	    print start_table{id=>'files'};
	    print colgroup col({id=>'size'}),col({id=>'name'}),col({id=>'meta',span=>3});
	    for (sort keys %{$dir->{''}}) {
		my $share=$dir->{''}{$_};
		my @meta;
		if ($share->{META}{bitrate}) {
		    push @meta, sprintf("%dkbps",$share->{META}{bitrate}/1000);
		}
		if ($share->{META}{duration} && $share->{META}{duration}>0) {
		    push @meta, sprintf("%d:%02d",map{$_/60,$_%60}$share->{META}{duration});
		}
		if ($share->{META}{width}) {
		    push @meta, sprintf("%d&nbsp;&#215;&nbsp;%d",@{$share->{META}}{'width','height'});
		}
		my $size=$share->{size};
		1 while $size=~s/(\d)(\d{3})(?!\d)/$1,$2/;
		
		print Tr 
		    td({class=>'size'},$size),
		    td({class=>'file'},a({href=>"http://$hostname:$port/".myescape"$path/$_"},escapeHTML $_)),
		    td{class=>'meta'},[
				       @meta,
				       ];
	    }
	    print end_table;
	}
    }
    print hr;
    print end_html;
}

sub myescape {
    # work around CGI.pm bug
    my $out=escape $_[0];
    $out=~s#%2F#/#g;
    $out;
}

sub cb_shares {
    for my $share ($shares->shares) {
	my ($path,$file)=$share->{hpath}=~m#^/(.*)/([^/]*)$#;

	my $pos=\%tree;
	
	for (split /\//,$path) {
	    my $newpos=$pos->{$_};
	    if (!$newpos) {
		$pos->{$_}={};
		$newpos=$pos->{$_};
	    }
	    $pos=$newpos;
	}
	$pos->{''}{$file}=$share;
    }
}
