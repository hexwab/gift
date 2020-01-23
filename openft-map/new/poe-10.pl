#!/usr/bin/perl

#sub POE::Kernel::ASSERT_DEFAULT () { 1 }

use strict;
use warnings;
use HTTP::Request::Common qw(GET);
use POE qw(Component::Client::HTTP);
$|++;

use constant MAX_SESSIONS => 10;

my $initial_node = {
    host =>'213.228.241.143',
    http_port => 1216,
    dummy => 1,
};

my $spider_user=0;

my %nodes;

sub info_to_key { my $i=shift; sprintf("%s:%d",@{$i}{'host','http_port'}) }

sub response {
    my ($kernel, $heap, $request_packet, $response_packet) =
	@_[KERNEL, HEAP, ARG0, ARG1];

    my $request  = $request_packet->[0];
    my $response = $response_packet->[0];
    my $info = $heap->{info};

    return unless $response->is_success();

    my $alias = $response->header('X-OpenftAlias');
    my $class = $response->header('X-Class');
    my ($min, $max) = map $response->header("X-Class$_"), qw[Min Max];
    my $body = $response->content();

    $info->{alias} = $alias;
    $info->{class} = $class if $class;
    $info->{min} = $min if $min;
    $info->{max} = $max if $max;
    $info->{spidered} = time;
    
    for my $line (split /\n/, $body) {
	my %info;
	@info{qw[host port http_port class ver]} = split' ', $line;
	$info{ver} = hex $info{ver};
	$info{parent} = info_to_key ($heap->{info}) if $info{class} & 256;

	$kernel->post (main => 'register_node', \%info);
    }
}

sub register_node {
    my ($kernel, $info)=@_[KERNEL, ARG0];
    
    my $key = info_to_key ($info);

#    printf "node: $key\n";

    my $node = $nodes{$key};
    
    if ($node) {
# FIXME
#	$kernel->post ($node->{session} => "update_info", 
    } else {
	POE::Session->create(
			     args => [ $info ],
			     inline_states => { 
				 _start => \&node_start,
				 throttle => \&throttle,
				 spider => \&spider,
				 response => \&response,
			     },
			     );
      }
}

sub throttle {
    my ($kernel, $heap)=@_[KERNEL, HEAP];

    my $count = $kernel->call('ua' => 'pending_requests_count');
    print STDERR "  $count         \r";

    if ($count < MAX_SESSIONS) {
	$kernel->yield ('spider');
    } else {
	$kernel->delay_add ('throttle', 5);
    }
}


sub spider {
    my ($kernel, $heap)=@_[KERNEL, HEAP];
    my $info = $heap->{info};

    my $url = sprintf "http://%s:%s/nodes", @{$info}{'host','http_port'};

#    print "requested $url\n";
    $kernel->post( ua => 'request', 'response', GET $url);
}

sub node_start {
    my ($kernel, $heap, $info)=@_[KERNEL, HEAP, ARG0];

#    $info->{created}=time;
    $info->{spidered}=0;
    $info->{class}&=7 if $info->{class};
    $heap->{info} = $info;
    $nodes{info_to_key($info)} = $heap;

    my $class=$info->{class};
    if (!defined $class or $spider_user or $class & 6) {
	$kernel->yield ('throttle');
    }
}

sub main_start {
    my $kernel = $_[KERNEL];

    $kernel->alias_set ('main');
    $kernel->yield (register_node => $initial_node);
}

POE::Component::Client::HTTP->spawn (Alias => 'ua', Timeout => 120);

POE::Session->create(
		     inline_states => {
			 _start => \&main_start,
			 register_node => \&register_node,
		     },
		     );

$poe_kernel->run();

my @keys=qw[host port http_port class min max ver created spidered alias parent];
my %keymap=map {($keys[$_],$_)} 0..$#keys;

for my $node (sort keys %nodes) {
    my $info=$nodes{$node}{info} or die;
    next if $info->{dummy};
    my $line;
    $line.="$_:$info->{$_} " for sort { $keymap{$a}<=>$keymap{$b} } keys %$info;
    chop $line;
    print "$line\n";
}
