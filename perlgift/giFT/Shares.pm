package giFT::Shares;

my %shares;

sub new {
    my (undef,$daemon,$lazy,$cb_sync,$cb_sync_state,$cb_hide,$cb_shares)=@_;
    # FIXME: differing laziness/callbacks
    return $shares{$daemon} if $shares{$daemon}; # singleton 

    my $self=bless {
	daemon=>$daemon,
	lazy=>$lazy,
	hidden=>undef,
	shares=>undef,
	sync_state=>undef,
	synced=>undef,
	dirty=>1,
	cb_sync=>$cb_sync, # called when sync completed
	cb_sync_state=>$cb_sync_state, # called when sync progress changes
	cb_hide=>$cb_hide, # called when hide state changes
	cb_shares=>$cb_shares, # called when shares list changes
    };
    my $cb=sub {
	my (undef,$m)=%{(shift)};
	for ($m->{action}) {
	    /hide|show/ and map {$_->[0]&&$_->[0]->($_->[1])}[$self->{cb_hide},$self->{hidden}=/hide/];
	    if (/sync/) {
		if ($m->{status} eq 'Done') {
		    $self->{synced}=1;
		    $self->{dirty}=1;
		    $self->{sync_state}=undef;
		    $self->{cb_sync}->() if $self->{cb_sync};
		} else {
		    $self->{sync_state}=$m->{status};
		    $self->{cb_sync_state}->($self->{sync_state}) if $self->{cb_sync_state};
		    $self->update_shares if !$self->{lazy};
		}
	    }
	}
	1;
    };

    $self->update_shares if !$self->{lazy};

    $daemon->set_handler('share',$cb);

    # request hidden status
    $daemon->put({share=>{action=>undef}}) if $daemon->attached;

    $self;
}

sub hidden_status {
    shift->{hide};
}

sub hide {
    shift->{daemon}->put({share=>{action=>$_[0]?'hide':'show'}});
}

sub sync {
    shift->{daemon}->put({share=>{action=>'sync'}});
}

sub sync_status {
    shift->{sync_state};
}

sub shares {
    @{shift->{shares}};
}

sub update_shares {
    my $self=shift;
    return if defined $self->{id}; # if already searching
    my $id='';
    my $daemon=$self->{daemon};

    $id=$daemon->get_id if ($daemon->attached);
    $self->{id}=$id;

    my $cb=sub {
	my $m=shift->{ITEM};

	if (keys%$m<2) {
	    my $f=$self->{cb_shares};
	    _cleanup_shares($self);
	    &$f($self) if defined $f;
	    undef $self->{id};
	    $self->{dirty}=0;
	    return 0;
	} else {
	    delete $m->{''};
	    push @{$self->{shares}}, bless $m,'giFT::Share';
	    return 1;
	}
    };

    $daemon->put({shares=>$id?{(''=>$id)}:undef});
    if ($id) {
	$daemon->set_handler_with_id('item',$id, $cb);
    } else {
	$daemon->set_handler('item', $cb);
    }
    
    $self;
}

sub _cleanup_shares {
    my $self=shift;

    my ($daemon,$id)=@{$self}{'daemon','id'};
    if ($id) {
	$daemon->set_handler_with_id('item',$id);
    } else {
	$daemon->set_handler('item');
    }

    $self->{finished}=1;
}

sub share_status {
    !shift->{dirty};
}

1;
__END__
=head1 NAME

giFT::Shares - manipulate files shared on giFT

=head1 SYNOPSIS

=head1 DESCRIPTION

=over 4

=item B<$shares=new giFT::Shares($daemon,$lazy,$callback)>

Creates a new shares object, and, if lazy is not set, requests a list
of shares from giFT.

Because this is expensive (especially with a remote giFT daemon), the
shares are cached where possible.  In particular, creating multiple
shares objects connected to the same daemon will not retrieve the
shares list twice.

Setting lazy will never retrieve shares automatically (call
update_shares() to retrieve them).

=head1 METHODS

=item B<shares()>

Returns the list of shares, or in scalar context, returns the number
of shares.

Returns undef if the shares list has yet to be retrieved, or if giFT
has yet to finish the initial synchronization.

(Note that calling this in scalar context should return the same value
as giFT::Stats::shared_stats under most circumstances, but the latter
is much more efficient due to not having to retrieve the shares list.)

=item B<find_by_hash()>

Returns the share(s) with the given hash.

Returns undef if the shares list has yet to be retrieved.

=item B<find_by_filename()>

Returns the share with the given hash.

Returns undef if the shares list has yet to be retrieved.

=item B<sync()>

Requests that giFT synchronize the shares list.

=item B<sync_status()>

Returns giFT's sync status value if giFT is currently synchronizing
shares, undef otherwise.

(Note that this is unrelated to whether files are currently shared or
not.)

=item B<sync_progress()>

Returns the progress of giFT's synchronization, as a percentage, or
undef if progress is unavailable.

=item B<hide($hide)>

Sets whether shares are hidden or not (boolean).

=item B<hidden_status()>

Returns true if shares are hidden, false otherwise, or undef if the
current state is unknown.

=item B<update_shares()>

Requests an up-to-date shares list from giFT. You should only need to
call this when lazy is true.

=item B<share_status()>

Returns true if the cached shares list is up-to-date.

=back

=head1 NOTES

This is implemented I<ickily>.

=head1 SEE ALSO

L<giFT::Daemon>, L<giFT::Search>, L<giFT::Interface>, L<giFT(1)>.

=head1 AUTHOR

Tom Hargreaves E<lt>HEx@freezone.co.ukE<gt>

=head1 LICENSE

Copyright 2003 by Tom Hargreaves.

This library is free software; you can redistribute it and/or modify it under the same terms as Perl itself. 

=cut
