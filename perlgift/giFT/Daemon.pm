package giFT::Daemon;

use giFT::Interface;
use Carp;
use strict;

sub new(;$$$$$) {
    my ($class,$host,$port,$blocking,$debug)=@_;
    use IO::Socket::INET;
    my $socket=new IO::Socket::INET(
	PeerAddr=>$host||'localhost',
	PeerPort=>$port||1213,
	Blocking=>$blocking,
	) or return undef;
    $socket->blocking($blocking); # grrr... the Blocking=>0 above doesn't seem to work!

    bless {
	socket=>$socket,
	left=>'',
	wait=>1,
	debug=>$debug,
	cb_id=>{},
	cb=>{},
	cb_default=>undef,
	blocking=>$blocking,
	attached=>0
	}, $class;
}

sub put($@) {
    my ($self,@commands)=@_;

    for my $command (@commands) {
	my $temp=giFT::Interface::serialize($command);
	print {$self->{socket}} $temp or die $!;
	print STDERR ">>> $temp" if $self->{debug};
    }
}

sub eof($) {
    my $self=shift;
    !$self->{wait} || do {
	use IO::Select;
	my $s=new IO::Select;
	$s->add($self->{socket});
	!$s->can_read(0);
    };
}

sub get($) {
    my $self=shift;
    my $line='';

#    print STDERR "get: wait=$self->{wait} left=".($self->{left})."\n";

#    die unless $self->{socket}->connected;

    if (!length($self->{left}) || $self->{wait}) {
	if (wantarray && !$self->{blocking}) { # FIXME!
	    local $/;
	    $line=$self->{socket}->getline;
	    $self->{wait}=0 if $line;
	} elsif (defined ($self->{socket}->recv($line,4096)) && length $line) {
	    $self->{wait}=0;
	}
	return () if $self->{wait};
    }

    $line=$self->{left}.$line if ($self->{left});

    if (!$line) {
	# not sure quite why this happens...
	$self->{wait}=1;
	return ();
    }

    if (wantarray) {
	my ($ret,$left)=giFT::Interface::unserialize_all($line);
	$self->{wait}=1;

	if ($ret) {
	    $self->{left}=$left;
	    print STDERR "<<< ".join("\n<<< ",split/\n/,substr($line,0,length($line)-length($left)))."\n"
		if $self->{debug};
	    return @$ret;
	}
    } else {
	my ($ret,$left)=giFT::Interface::unserialize($line);
	if ($ret) {
	    $self->{left}=$left;
	    print STDERR "<<< ".substr($line,0,length($line)-length($left))
		if $self->{debug};
	    return $ret;
	}
	$self->{wait}=1;
    }

    $self->{left}=$line;
    
    ();
}

sub get_socket($) {
    shift->{socket};
}

sub DESTROY {
    my $self=shift;
    print STDERR "[daemon destroyed]\n" if $self->{debug};
    return if !defined $self->{socket};
    $self->detach();
    return $self->{socket}->shutdown(2);
}


sub poll($) {
    my $self=shift;
    for my $m ($self->get) {
	die if !$m;
	my ($type)=keys %$m;
	my $id=$m->{$type}{''};
	$type=lc $type;
	my $func=undef;
	if (defined $id &&
	    exists $self->{cb_id}{$type} &&
	    exists $self->{cb_id}{$type}{$id}) {
	    $func=\$self->{cb_id}{$type}{$id};
	}
	if (!defined $$func && exists $self->{cb}{$type}) {
	    $func=\$self->{cb}{$type};
	}
	if (!defined $$func) {
	    $func=\$self->{cb_default};
	}

	if (defined $$func) {
	    if (!&$$func($m)) {
		# FIXME: stop filling up the hashes with empty values
		undef $$func;
	    }
	} else {
	    warn "No handler for $type".((defined $id)?", id $id":'') if $self->{debug};
# 	    use Data::Dumper;
#	    print STDERR Dumper $self;
	}
    }
}

sub set_handler($$;$) {
    my ($self,$type,$func)=@_;
    _set_handler($self,$self->{cb},lc$type,$func);
}

sub set_handler_with_id($$$;$) {
    my ($self,$type,$id,$func)=@_;
    undef $self->{cb_id}{lc$type}{$id};
    _set_handler($self,$self->{cb_id}{lc$type},$id,$func);
}

sub set_default_handler($;$) {
    my ($self,$func)=@_;
    if (defined $func) {
	croak 'Invalid handler' unless (ref $func eq 'CODE');
    }
    $self->{cb_default}=$func;
}

sub _set_handler {
    my ($self,$hash,$loc,$func)=@_;
    if (defined $func) {
	croak 'Invalid handler' unless (ref $func eq 'CODE');
	$hash->{$loc}=$func;
    } else {
	delete $hash->{$loc};
    }
}

sub get_id($) {
    ++shift->{id};
}

sub attach($;$$$) {
    my ($self,$app,$ver,$attach_cb)=@_;
    return if $self->{attached};
    warn 'Skipping version callback' if $self->{cb}->{attach} && $self->{debug};
    $self->set_handler('attach', sub {
	my @a=@{shift->{ATTACH}}{'version','server'};
	@{$self}{'version','server'}=@a;
	&$attach_cb(@a) if $attach_cb;
	0;
    }) unless $self->{cb}->{attach}; # don't clobber an existing handler

    $self->put({attach=>{
	(defined $app)?(client=>$app):(),
	(defined $ver)?(version=>$ver):(),
    }});
    $self->{attached}=1;
}

sub detach($) {
    my $self=shift;
    return if !$self->{attached};
    $self->put({detach=>undef});
    $self->{attached}=0;
}

sub attached($) {
    shift->{attached};
}

sub version {
    my $self=shift;
    return $self->{version} unless wantarray && $self->{attached};
    @{$self}{'version','server'};
}    

1;
__END__
=head1 NAME

giFT::Daemon - communication with giFT daemon

=head1 SYNOPSIS

  use giFT::Daemon;

  my $daemon=new giFT::Daemon;

  my $daemon=new giFT::Daemon("localhost",1213); # same as above

  $daemon->put({search=>{query=>"test"}});

  while (1) {
     my $result=$daemon->get;
     ...
  }

=head1 DESCRIPTION

Connects to a giFT daemon; by default one listening on port 1213
on the local machine.

=head1 METHODS

=head2 INPUT/OUTPUT

=over 4

=item B<new($host,$port,$blocking,$debug)>

Opens a connection to a giFT daemon at the specified address. If
blocking is set, the socket is allowed to block; this is probably only
of use in either very simple applications, or applications using
threads.

If debug is set, all communication is logged to stderr (and other
miscellaneous conditions).

=item B<put($command)>

Encodes a command using giFT::Interface, and sends it to the daemon.

=item B<get()>

Gets a single result from the daemon, and decodes it using
giFT::Interface. Returns undef if no results are waiting.

When called in array context, returns all the results waiting.

Works slightly differently in blocking mode: if no results are
waiting, returns undef the first time it's called, and blocks on the
next call until results arrive. This has the side-effect that poll()
(which uses get() internally) will return once all the waiting results
have been processed, without blocking.

=item B<eof()>

Returns true if no results are waiting, false otherwise.

=item B<get_socket()>

Returns the internal IO::Socket::INET object. Useful when using your
own select() loop.

=back

=head2 EVENT HANDLING

Callback handlers allow easy handling of results.
Only one handler is called per result - more specific handlers get
priority. The precedence is the same as in the method list below.

One parameter is provided: the result as a hash reference. If the
handler returns false, it will not be called again.

=over 4

=item B<poll()>

Checks for incoming results by calling get() repeatedly, and
dispatches them to the appropriate callback handler(s).

=item B<set_handler_with_id($command, $id, $handler)>

Sets the callback handler for the given command type, but only when it
has the given id. Use undef to remove the current handler.

=item B<set_handler($command, $handler)>

Sets the callback handler for the given command type. Use undef to
remove the current handler.

=item B<set_default_handler($handler)>

Sets the default callback handler for unknown command types, or for
commands with no matching id. Use undef to remove the current handler.

=back

=head2 MISCELLANEOUS

=over 4

=item B<get_id()>

Returns a unique number suitable for use as an id.

=item B<attach($appname,$version,$callback)>

Sends an ATTACH command to the daemon. It's a good idea to use this
and the following method instead of sending the commands manually, as
some packages may rely upon knowing whether a session is attached or
not.

The callback, if specified, will be called with the name and version
returned by giFT.

=item B<detach()>

Sends a DETACH command to the daemon.

=item B<attached()>

Returns true if a session is attached.

=item B<version()>

Returns the version number of the giFT daemon, if a session is
currently attached.  When called in array context, returns the
daemon's "server name" too. (Previous versions of this document stated
"this should be 'giFT' for the forseeable future", but it's since
changed to 'gift' and again to 'giftd'...)

=back

=head1 SEE ALSO

L<giFT::Interface>, L<giFT::Search>, L<giFT(1)>.

=head1 AUTHOR

Tom Hargreaves E<lt>HEx@freezone.co.ukE<gt>

=head1 LICENSE

Copyright 2003 by Tom Hargreaves.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
