package giFT::Stats;

my $stats_obj;

sub new {
#    return $stats_obj if $stats_obj;
    shift;
    my $daemon=shift;

    my $self=bless {
	daemon=>$daemon,
	protos=>{},
	connecting=>1,
	total=>{},
	shared=>{},
	cb1=>shift,
	cb2=>shift,
	time=>0,
    };

    my $cb=sub {
	$self->{time}=time;
	my (undef,$m)=%{(shift)};
	use Data::Dumper;
	my $need_update=0;
	if ($self->{cb2}) {
	    my %protos=('giFT'=>1,%{$self->{protos}});
	    for (keys %$m) {
		$need_update=1, last if !$protos{$_};
		delete $protos{$_};
	    }
	    $need_update=1 if %protos;
	}
	
	my %totals;

	$self->{connecting}=0;
	for my $proto (keys %$m) {
	    $m->{$proto}{size}*=1<<30;
	    if ($proto ne 'giFT') {
		$self->{connecting}=1 if !$m->{$proto}{users};
		$totals{$_}+=$m->{$proto}{$_} for keys %{$m->{$proto}};
	    }
	}

	$self->{shared}=$m->{giFT};
	delete $m->{giFT};

	$self->{cb2}->(keys %$m) if $need_update;

	$self->{protos}=$m;

	$self->{total}=\%totals;

	$self->{cb1}->() if $self->{cb1};
	1;
    };

    $self->{daemon}->set_handler('STATS',$cb);

    $self->pump;

    return $stats_obj=$self;
}

sub proto_stats {
    my $self=shift;
    my $proto=shift;
    return () if $proto && !$self->{protos}{$proto};

    return @{$proto?$self->{protos}{$proto}:$self->{total}}{'users','files','size'};
}

sub protocols {
    return keys %{shift->{protos}};
}

sub connected_protocols {
    my $self=shift;
    return grep {$self->{protos}{$_}{users}} keys %{$self->{protos}};
}

sub shared_stats {
    return @{shift->{shared}}{'files','size'};
}

sub pump {
    my $self=shift;
    return if $self->{time} && (time-$self->{time}<(($self->{connecting})?3:30));
    $self->{daemon}->put({stats=>undef});
}

1;
__END__
=head1 NAME

giFT::Stats - giFT statistics

=head1 SYNOPSIS

  use giFT::Stats;

  my $stats=new giFT::Stats($daemon);

  my ($users, $files, $bytes)=$stats->proto_stats();

=head1 DESCRIPTION

Gathers statistics from giFT.

=head1 METHODS

=over 4

=item B<new($daemon,$update_callback,$proto_callback)>

Installs a handler to process STATS messages from the daemon.

The update callback is called with no parameters when giFT sends a
STATS message.

The proto callback is called with a list of protocols whenever a
protocol is added or removed. Currently this is only called once, when
first attaching to a daemon, but might be called at other times if
giFT ever supports run-time protocol loading/unloading. 

=item B<protocols()>

Returns a list of currently active protocols.
Note that the "giFT" pseudo-protocol is not included in this list.

=item B<connected_protocols()>

Returns a list of currently connected protocols (i.e. those with a user
count greater than zero).

=item B<proto_stats($proto)>

Returns a list containing (users, files, bytes) for the specified
protocol, or the total of all protocols if $proto is undef.

=item B<shared_stats()>

Returns a list containing the number of locally shared files and their
total filesize in bytes.

=item B<pump()>

Asks giFT to send some stats. This should be called periodically to
keep the stats up-to-date.

=back

=head1 SEE ALSO

L<giFT::Daemon>, L<giFT::Shares>, L<giFT(1)>.

=head1 AUTHOR

Tom Hargreaves E<lt>HEx@freezone.co.ukE<gt>

=head1 LICENSE

Copyright 2003 by Tom Hargreaves.

This library is free software; you can redistribute it and/or modify it under the same terms as Perl itself. 

=cut
