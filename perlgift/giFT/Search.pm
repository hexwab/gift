package giFT::Search;

use giFT::Daemon;
use Carp;
use strict;

sub new($$$;$$$$) {
    my (undef,$daemon,$method,$query,$result_func,$finished_func,$keep_hash)=@_;

    my $id;
    $id=$daemon->get_id if ($daemon->attached);

    my $cmd={
	$method=>
	    {
		($method=~/^shares$/i)?():(query=>$query),
		$id?(''=>$id):(),
	    },
    };
    
    my $self=bless{
	daemon=>$daemon,
	method=>$method,
	query=>$query,
	id=>$id,
	results=>[],
	duphash=>{},
	finished=>0,
	result_cb=>$result_func,
	finished_cb=>$finished_func,
	keep_hash=>$keep_hash,
	unique=>0,
    };

    my $cb=sub {
	# in case results are waiting after we cancelled
	return 1 if ($self->{finished});
	# (not zero, as we don't want to send spurious results elsewhere)

	my $m=shift->{ITEM};

	die if !$self->{daemon};

	if (keys%$m<2) {
	    my $f=$self->{finished_cb};
	    _cleanup($self);
	    &$f($self) if defined $f;
	    return 0;
	} else {
	    local $^W=0;
	    my $hash=$m->{hash};
	    my $user=$m->{user};
	    $self->{unique}++ if !exists $self->{duphash}{$hash};
	    return 1 if exists $self->{duphash}{$hash}{$user}; # duplicate
	    $self->{duphash}{$hash}{$user}=$m;
	    push @{$self->{results}}, $m;
	    my $f=$self->{result_cb};
	    &$f($self, $m) if $f;
	    return 1;
	}
    };

    $daemon->put($cmd);
    if ($id) {
	$daemon->set_handler_with_id('item',$id, $cb);
    } else {
	$daemon->set_handler('item', $cb);
    }
    
    $self;
}

sub finished($) {
    shift->{finished};
}

sub results($) {
    @{shift->{results}};
}

sub unique_results($) {
    shift->{unique};
}

sub cancel($) {
    my $self=shift;
    return if $self->{finished};
    my $cmd={$self->{method}=>{
	((defined $self->{id})?(''=>$self->{id}):()),
	cancel=>undef,
	}};
    $self->{daemon}->put($cmd);
    
    _cleanup($self);
}

sub find_results_by_hash($$) {
    my ($self,$hash)=@_;
    if (defined $self->{duphash}) {
	return values %{$self->{duphash}{$hash}};
    }
    carp "find_results_by_hash() called with no hash table available";
    undef;
}

sub _cleanup($) {
    my $self=shift;

    my ($daemon,$id)=@{$self}{'daemon','id'};
    if ($id) {
	$daemon->set_handler_with_id('item',$id);
    } else {
	$daemon->set_handler('item');
    }

    delete $self->{$_} for qw[daemon temp result_cb finished_cb];
    $self->{finished}=1;
}

sub data($$) : lvalue {
    my ($self,$result)=@_;
    $self->{data}{$result};
}

sub DESTROY {
    shift->cancel;
}

1;
__END__
=head1 NAME

giFT::Search - search using giFT

=head1 SYNOPSIS

  use giFT::Daemon;
  use giFT::Search;

  # create a new blocking daemon connection
  my $daemon=new giFT::Daemon(undef,undef,1);

  $daemon->attach; # giFT bug workaround

  my $search=new giFT::Search($daemon,
			      search=>'test',
			      \&search_result,
			      \&search_done);

  do {
      $daemon->poll;
  } while (!$search->finished);

  my $results=$search->results;

  print "Got $results results\n";

  sub search_result {
      my ($search, $result)=@_;
      print "Search result: $result->{file}\n";
  }

  sub search_done {
      my ($search)=@_;
      print "Search finished\n";
  }


=head1 DESCRIPTION

=over 4

=item B<$search=new giFT::Search($daemon,$method,$query,$result_handler,$finished_handler,$keep_hash)>

Sends a search query to giFT. Method should be one of 'search',
'locate', 'browse' or 'shares' - see the giFT interface protocol
documentation for details. All except 'shares' require a query.

The callback handlers are called when a search result arrives and when
the search completes, respectively. The search object is passed as a
parameter; the result handler also gets the result.

Duplicate results are automatically deleted, and do not call the
result handler.

The keep_hash parameter must be non-zero if you plan to call
find_result_by_hash() on a completed search.

=head1 METHODS

=item B<results()>

Returns an array of results received so far, in the usual
giFT::Interface format, except missing the "ITEM" command. When called
in scalar context, returns the number of results received.

=item B<unique_results()>

Returns the number of unique (i.e. having distinct hashes) results
received so far.

=item B<finished()>

Returns true if the search has completed (or has been cancelled).

=item B<cancel()>

Aborts the search. Sets the "finished" flag, but doesn't call the
finished handler. The results handler is guaranteed not to be called
again, even if results are waiting.

=item B<find_results_by_hash($hash)>

Returns an array of results matching the given hash. In scalar context
returns the number of entries the array would have contained. Will
always return undef (and produce a warning) if the search has
completed unless the keep_hash parameter was set when the search was
initialized.

Note: The order of the array results is B<not> guaranteed.

=item B<data($result)>

Allows you to associate arbitrary data with a search result. Returns
an lvalue.

=back

=head1 NOTES

Results are discarded as duplicates if their hash and user both match a
previous result. This may result in lost results if a user is sharing
two or more identical files.

L<giFT::Shares> should be used instead of the 'shares' method for a
higher-level interface to giFT's shares list.

=head1 WARNING

Due to a bug in giFT, unattached searches cause unpredictable effects:
closing the connection prematurely, hanging or crashing the daemon.

Workaround: attach to the daemon first (see above).

=head1 SEE ALSO

L<giFT::Daemon>, L<giFT::Interface>, L<giFT::Shares>, L<giFT(1)>.

=head1 AUTHOR

Tom Hargreaves E<lt>HEx@freezone.co.ukE<gt>

=head1 LICENSE

Copyright 2003 by Tom Hargreaves.

This library is free software; you can redistribute it and/or modify it under the same terms as Perl itself. 

=cut
