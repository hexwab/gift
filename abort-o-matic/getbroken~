#!/usr/bin/perl -w

sub getnode;
sub stats;
	    
my @nodes;

my $halfconns;

# definitions: "primary" is a node we actually spidered. "secondary"
# is a node we didn't spider, but that one or more primary nodes is
# connected to (thus we can infer at least some of its connections).
# A "half-connection" is a connection that only one end can verify. We
# drop these. This can only occur between primaries; we assume all
# secondary connections are valid.

# automatic gunzip
s/(.*)\.gz$/gzip -dc $1|/ for @ARGV;

# read map
while (<>) {
    my $node=getnode(1) or die;
#    print "node:$node class:".$nodes{$node}{class}." ver:$nodes{$node}{ver}\n";
    while (my $peer=getnode(0)) {
	if ($nodes{$peer}{primary} &&
	    !grep {$_ eq $node} @{$nodes{$peer}{conns}}) {
	    $halfconns++;
	} else {
	    if (grep {$_ eq $peer} @{$nodes{$node}{conns}}) {
		# due to a bug in the spider, some connections are
		# listed twice (SEARCH nodes also being CHILD nodes)

		#warn "Duplicate connection: $node->$peer\n";
	    } else {
		push @{$nodes{$node}{conns}}, $peer;
		$conns++ unless $nodes{$peer}{primary};
	    }
	}
    }
}

# fixup connections for secondary nodes
for my $node (keys %nodes) {
    next unless ${nodes}{$node}{primary};
    for my $peer (@{$nodes{$node}{conns}}) {
	next if $nodes{$peer}{primary};
	die "$peer" unless $nodes{$peer}{class};
	die "$peer: $node:[".join',',@{$nodes{$peer}{conns}}
	    if grep {$_ eq $node} @{$nodes{$peer}{conns}};
	push @{$nodes{$peer}{conns}},$node;
    }
}

my $nodecount=keys %nodes;

# class stats
for my $bit (0..2) {
    my $count=grep {$_->{class} & (1<<$bit)} values %nodes;
    print [qw[USER SEARCH INDEX]]->[$bit].": $count\n";
}

print "Total connections: $conns\n";
printf "Half-open connections: %d (%.2f%%)\n",$halfconns,$halfconns/$conns*100;

#leaf nodes
{ 
    my %user=map {$_,$nodes{$_}} grep {$nodes{$_}{class}==1} keys %nodes;
    printf "Leaf nodes: %d (%.2f%%)\n",scalar keys %user, (scalar keys
							  %user)/$nodecount*100;
    
    printf "Connections: mean %.2f, median %d, mode %d, min %d, max %d\n",
        stats map {scalar @{$_->{conns}}} values %user;

    # ignore them hereafter
    for my $node (keys %nodes) {
	next if $nodes{$node}{class}==1;
	# not terribly accurate child count attempt
	$nodes{$node}{children}=scalar grep {$nodes{$_}{class}==1} @{$nodes{$node}{conns}};
	$nodes{$node}{conns}=[grep {$nodes{$_}{class}!=1} @{$nodes{$node}{conns}}];
    }
}

#search nodes
{
    my %search=map {$_,$nodes{$_}} grep {$nodes{$_}{class} &2} keys %nodes;
    printf "Search nodes: %d (%.2f%%)\n",scalar keys %search, (scalar keys
							  %search)/$nodecount*100;
    
#    print join',', map {scalar @{$_->{conns}}} values %search;
#    print "\n\n";
    printf "Children: mean %d, median %d, mode %d, min %d, max %d\n",
        stats map {$_->{children}} values %search;

    printf "Connections: mean %.2f, median %d, mode %d, min %d, max %d\n",
        stats map {scalar @{$_->{conns}}} values %search;
    
# print empty search nodes
    for (grep !$search{$_}{children}, keys %search) { 
	if ($search{$_}->{ver}<0x20103 && $search{$_}->{port}==1216) {
	    printf "%s\n", $_;
	}
    }
    exit;
    # network topology stats here we come

    my $ttl=0;
    my $done=0;	
    my $netsplit;
    print "       Network coverage (%)       Duplicates (%)              Messages\n";
    print "TTL  mean median  min   max   mean median  min   max   mean median  min   max\n";

    while (!$done && $ttl<10) {
	$ttl++;
	my @dups;
	my @coverage;
	my @msgs;
	$done=1;

	# broadcast a message starting from each node in turn
	for my $start (keys %search) {
	    my %reached;
	    my $hops=0;
	    my @msg=({node=>$start});
	    $isolated++,next if !@{$nodes{$start}{conns}};
	    my $dups=0;
	    my $msgs=0;
	    while (keys %reached!=keys %search && @msg) {
		my $maybesplit;
		my @newmsg;
		for my $msg (@msg) {
		    $msgs++;
		    my $node=$msg->{node};
		    push @{$reached{$node}},$hops;
		    my $parent=$msg->{parent};
		    # don't propagate if this message has already reached us
		    if (@{$reached{$node}}==1) {
			$maybesplit++;
			if ($hops<$ttl) {
			    for my $peer (@{$nodes{$node}{conns}}) {
				# skip the node we got it from
				next if $parent && $peer eq $parent;
				next unless $nodes{$peer}{class} & 2;
				push @newmsg, {node=>$peer, parent=>$node};
			    }
			}
		    } else {
			$dups++;
		    }
		}
		@msg=@newmsg;
		$hops++;
#		print "start $start, hops $hops, msgs ".(scalar @msg)." reached ".(scalar keys %reached)."\n";
		$netsplit++ if (!$maybesplit && (keys %reached < keys %search));
	    }
	    $done=0 if keys %reached != keys%search && !$netsplit;
	    push @coverage,keys(%reached)/keys(%search)*100;
	    push @dups, $dups/$msgs*100;
	    push @msgs,$msgs;
#	    printf "%-15s coverage %.2f%% messages %d dups %d (%.2f%%)\n", $start, keys(%reached)/keys(%search)*100,$msgs, $dups, $dups/$msgs*100;
	}
	printf "%-2d  %5.1f %5.1f %5.1f %5.1f  %5.1f %5.1f %5.1f %5.1f  %5d %5d %5d %5d\n", 
	    $ttl, (stats @coverage)[0,1,3,4], (stats @dups)[0,1,3,4], (stats @msgs)[0,1,3,4];
    }

    printf "%d disconnected nodes skipped (%.2f%%)\n",$isolated,100*$isolated/keys%search if $isolated;
    print "Network is split!\n" if $netsplit;

}
print "\n";


sub getnode($) {
    my $primary=shift;
    s/([\d\.]+):(\d+)\(0x(\d+):0x(\d+)\),? *//
	or return undef;
    my ($node,$port,$class,$ver)=($1,$2,$3,$4);

    $_=hex for ($class,$ver);
#    die sprintf"%x",$class if $class & 0xf8f8;
    $class&=7;
    
    my $n=$nodes{$node};
    $nodes{$node}{primary}++ if $primary;
    if ($n) {
#	warn "[class] $n->{class}!=$class ($node)"
#	    unless $n->{class}==$class;
	$n->{class}&=$class;
	warn "[port] $n->{port}!=$port ($node)"
	    if $port && $n->{port} &&
		$n->{port}!=$port;
	#die unless $n->{ver}==$ver;
	#version mismatches do occur; dunno why
	$n->{port}=$port if $port;
	$n->{ref}++;
    } else {
	@{$nodes{$node}}{'class','ver','port'}=($class,$ver,$port);
	$nodes{$node}{fw}++ if !$port;
    }
    return $node;
}

sub stats {
    return ((undef) x 6) unless @_;
    my $mean; $mean+=$_ for @_;
    my $ssize=@_;
    $mean/=@_;
    my ($min,$median,$max)=(sort {$a<=>$b} @_)[0,$#_/2,$#_];
    my %h;$h{$_}++ for @_;
    my $mode=(sort {$h{$b}<=>$h{$a}} @_)[0];
    return ($mean,$median,$mode,$min,$max,$ssize);
}
