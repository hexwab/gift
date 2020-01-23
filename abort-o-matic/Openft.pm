###############################################################################
##
## $Id: Openft.pm,v 1.8 2003/05/31 09:52:33 jasta Exp $
##
## Copyright (C) 2003 giFT project (gift.sourceforge.net)
##
## This program is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by the
## Free Software Foundation; either version 2, or (at your option) any
## later version.
##
## This program is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
###############################################################################

package Openft;

###############################################################################

use strict;

use IO::Socket::INET;

@Openft::version=(0,0,9,7);

###############################################################################

sub new
{
	my $class = shift;

	my $self = bless ({}, $class);

	$self->_init (@_);

	return $self;
}

sub _init
{
	my $self = shift;
	my $node = shift;

	$self->{node} = $node;
	$self->set_err (undef);
}

sub DESTROY
{
	my $self = shift;

	$self->disconnect;
}

###############################################################################

sub set_err
{
	my $self = shift;

	$self->{err} = shift;
}

sub get_err
{
	my $self = shift;

	return $self->{err} || $! || "???";
}

###############################################################################

sub connect
{
	my $self = shift;

	die "No node selected" unless $self->{node};

	my $sock = IO::Socket::INET->new ('PeerAddr' => $self->{node},
	                                  'Timeout'  => 15);
	return 0 unless defined $sock;

	$self->{sock} = $sock;
	$self->{nodever} = $self->_verneg;

	if ($self->{nodever} == 0)
	{
		$self->set_err ('connection terminated');
		return 0;
	}

	if ($self->{nodever} < 0x00000906)
	{
		$self->set_err (sprintf ('version mismatch %08x', $self->{nodever}));
		return 0;
	}

	1;
}

sub disconnect
{
	my $self = shift;

	close ($self->{sock});
	delete $self->{sock};
}

###############################################################################

sub _waitio
{
	my $self   = shift;
	my $dir    = shift;
	my $method = sprintf ("can_%s", $dir);

	# initialize the select() syscall wrapper...
	my $select = IO::Select->new ($self->{sock});

	# 15 second can_read or can_write timeout according to $dir...
	if (!($select->$method (15)))
	{
		$self->set_err ("$dir timeout");
		return 0;
	}

	1;
}

sub _sendpkt
{
	my $self = shift;
	my $sock = $self->{sock};
	my $pkt  = shift;

	die if (defined $pkt->{data} && $pkt->{len} == 0);
	die if (!(defined $pkt->{data}) && $pkt->{len} > 0);

	my $buf = pack ("nna*", $pkt->{len}, $pkt->{cmd}, $pkt->{data});

	# wait for writing...
	return undef unless $self->_waitio ('write');

	die "size mismatch" unless (length $buf == (4 + $pkt->{len}));

	# perform the actual send
	my $n = send ($sock, $buf, 0);
	die "$!" unless $n > 0;

	return $n;
}

sub _readpkt
{
	my $self = shift;
	my $sock = $self->{sock};
	my $pkt  = {};

	my $buf;

	# wait for reading...
	return undef unless $self->_waitio ('read');

	# perform the actual read
	recv ($sock, $buf, 4, 0);
	return undef unless length $buf == 4;

	($pkt->{len}, $pkt->{cmd}) = unpack ('nn', $buf);

	if ($pkt->{len} > 0)
	{
		recv ($sock, $pkt->{data}, $pkt->{len}, 0);
		return undef unless length $pkt->{data} == $pkt->{len};
	}

	return $pkt;
}

###############################################################################

sub _verneg
{
	my $self = shift;

	# send the version request so that we can get their remote ver
	$self->_sendpkt ({ 'len' => 0, 'cmd' => 0x00 });

	# receive the version negotiation request and respond
	my $pkt = $self->_readpkt ();
	die unless $pkt->{cmd} == 0x00;

	my $verreply = pack ('nnnn', @Openft::version);
	$self->_sendpkt ({ 'len' => 8, 'cmd' => 0x01, 'data' => $verreply });

	# receive the version response from the request we sent above
	my $pkt = $self->_readpkt ();
	return 0 unless defined $pkt;

	die unless $pkt->{cmd} == 0x01;

	my ($maj, $min, $mic, $rev) = unpack ('nnnn', $pkt->{data});

	# produce the same version identifier that OpenFT uses internally
	return ((($maj & 0xff) << 24) |
	        (($min & 0xff) << 16) |
	        (($mic & 0xff) << 8) |
	        (($rev & 0xff)));
}

###############################################################################

sub send_nodes
{
	my $self = shift;
	my $pkt;

	while (1) {
	    my $list;
	    my $n;
	    for (0..19) {
		$n=shift;
		$list .= pack ("na4nn", 4, @$n) if $n;
	    }
#	open(F,"|hd") or die; print F $list;close F;
	    $self->_sendpkt ({ 'len' => length $list, 'cmd' => 0x05, 'data' => $list });
	    return if !$n;
	}
}

sub _get_nodes
{
	my $self = shift;
	my $klass_filter = shift;
	my $pkt;
	my @list;
	my $nodes = {};

	my $listreq = pack ("nn", $klass_filter, 0);
	$self->_sendpkt ({ 'len' => 4, 'cmd' => 0x04, 'data' => $listreq });

	while (($pkt = $self->_readpkt()))
	{
		# shift responses off until we get the nodelist response packet we're
		# after...
		last if $pkt->{cmd} == 0x05;
	}

	return () if ($pkt->{len} == 0);

	# parse every node in the list response
	for (my $offs = 0;; $offs += 10)
	{
		last if ($offs >= length ($pkt->{data}));
		my $data_seg = substr ($pkt->{data}, $offs, 10);

		my ($ipver, $ip, $port, $klass) = unpack ("nNnn", $data_seg);
		die unless $ipver == 4;

#		next unless ($klass & $klass_filter);

		my $noderef = { 'ip'    => inet_ntoa (pack ("N", $ip)),
		                'port'  => $port,
		                'klass' => $klass };

		# prevent multiple entries from the same ip address...
#		next if exists $nodes->{$noderef->{ip}};
		$nodes->{$noderef->{ip}} = {};

		push @list, $noderef;
	}

	return @list;
}

sub get_child_nodes
{
	my $self = shift;

	return $self->_get_nodes (0);
}

sub get_search_nodes
{
	my $self = shift;

	return $self->_get_nodes (0x02);
}

###############################################################################

sub get_node_info
{
	my $self = shift;
	my $pkt;

	my $req = pack ("N", 0);
	$self->_sendpkt ({ 'len' => 4, 'cmd' => 2, 'data' => $req });

	while (($pkt = $self->_readpkt()))
	{
		# shift responses off until we get the nodelist response packet we're
		# after...
		last if $pkt->{cmd} == 0x03;
	}

	return () if ($pkt->{len} == 0);

	my (undef, $ip, $class, $port, $hport, $alias) = unpack ("nNnnnZ*", $pkt->{data});

	return {ip=>$ip, class=>$class, port=>$port, hport=>$hport, alias=>$alias};
}


1;
