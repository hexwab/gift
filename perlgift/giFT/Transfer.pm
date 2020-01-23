package giFT::TransferHandler;

my @handlers=qw(
		ADDUPLOAD CHGUPLOAD DELUPLOAD
		ADDDOWNLOAD CHGDOWNLOAD DELDOWNLOAD
		);

sub new {
    my $daemon=shift;
    my $self=bless {
	daemon=>$daemon,
	upload=>{},
	download=>{},
	width=>{},
	handlers=>{ map {$_=>shift} @handlers },
    }

    my $cb=sub {
	local $_;
	my ($c,$m)=%{(shift)};
	my $hash;
	my $id=$m->{''};
	for ($c) {
	    /DOWN/ and $hash=$self->download;
	    /UP/ and $hash=$self->upload;
	    
	    if (/ADD/) {
		$m->{_ctime}=time;
		bless $hash->{$id}=$m,'giFT::Transfer';
	    } elsif (/CHG/) {
		$hash->{$id}={%{$hash->{$id}},%$m};
		my $w=$width->{$id};
		$w=$width->{$id}=[] if !defined $w;
		push @$w,$m->{throughput};
		shift @$w if @$w>5;
		my $width=0; $width+=$_ for @$w;
		$width/=@$w;
		$hash->{$id}{throughput}=$width;
	    } else {
		my $done=($uploads{$id}{SOURCE}{transmit}==$uploads{$id}{SOURCE}{total});
		$hash->{$id}{throughput}=0;
		$hash->{$id}{completed}=$done;
		$hash->{$id}{state}=$done?'Complete':'Cancelled'; # ewwwww
		delete $width->{$id};
		s/DEL/CHG/;
	    }
	}
	map {$_&&&$_($hash->{$id})} $self->{handlers}{$c};
	1;
    };

    $self->_set_handler($cb);

    $self;
}

sub DESTROY {
    my $self=shift;
    $self->_set_handler(undef);
}

sub _set_handler {
    my ($self,$cb)=@_;
    for my $handler (@handlers) {
	    $self->{daemon}->set_handler($handler=>$cb);
	}
}

sub clear_completed {
    
}

sub bandwidth {
    
    return ($up,$down);
}


package giFT::Transfer;
use vars '@ISA';
@ISA='giFT::Transfer';

sub cancel {
}

sub bandwidth {
}

sub status {
}
