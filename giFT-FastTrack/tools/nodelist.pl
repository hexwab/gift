#!/usr/bin/perl -w
my %h;
my $thishost='';
my ($yep,$total);
my $thresh=300;
my (@nodes,@zero);
while (<>) {
    chomp;
    if (!$_) {
	if (defined ($h{$thishost}{peers})) {
	    push @nodes,scalar keys %{$h{$thishost}{peers}};
	    push @zero,scalar grep !$_, values %{$h{$thishost}{peers}};
	}
	$thishost='';
	next;
    }
   
    my ($time, $host, $node, $load, $last)=split' ';
    die $_ unless $time && $host && $node && defined $load && defined $last;

    $time=hex $time;

    die "'$thishost' '$host'" if $thishost && $thishost ne $host;

    if (!$thishost)
    {
	$h{$host}{peers}={};
	$thishost=$host;
#	print "$host: ".($time-$h{$host}{last})."\n" if $h{$host}{last};
	$h{$host}{last}=$time;
    }

    die "duplicate $host $node" if $host ne $node && defined $h{$host}{peers}{$node};
    $h{$host}{peers}{$node}=$last;
    warn if keys %{$h{$host}{peers}}>200;

    if ($h{$node} && $h{$node}{last}
	&& $time-$h{$node}{last}<$thresh
	&& $last!=0
	) {
	if (defined $h{$node}{peers}
	    &&keys %{$h{$node}{peers}}
	    ) {
	    $total++;
	    if (
#		$h{$node}{peers}{$host}==0
#		&& 
		defined $h{$node}{peers}{$host}
		) {
		$yep++;
	    }
	}
    }
}
printf "%d/%d (%.2f%%, thresh %d)\n", $yep,$total,($total&&$yep/$total*100),$thresh;
printf "%d lists, %.2f avg nodes, %.2f avg peers\n", scalar @nodes, 
    do{my $i;map $i+=$_,@nodes;$i}/@nodes,
    do{my $i;map $i+=$_,@zero;$i}/@zero;
