package giFT::TransferHandler;

my @handlers=qw(
		ADDUPLOAD CHGUPLOAD DELUPLOAD
		ADDDOWNLOAD CHGDOWNLOAD DELDOWNLOAD
		);

sub new {
    my (undef,$daemon,$autoclear,$mergeuploads,@h)=@_;
    my $self=bless {
	daemon=>$daemon,
	upload=>{},
	download=>{},
	width=>{},
	handlers=>{ map {$_=>shift @h} @handlers },
	uplinks=>{},
	autoclear=>$autoclear,
	mergeuploads=>$mergeuploads,
    };

    my $cb=sub {
	local $_;
	my ($c,$m)=%{(shift)};
	my $hash;
	my $realid=$m->{''};
	my $id=$self->{uplinks}{$realid} || $realid;
	$m->{''}=$id;
      TRANSFER:
	for ($c) {
	    /DOWN/ and $hash=$self->{download};
	    /UP/ and $hash=$self->{upload};
	    
	    if (/ADD/) {
		if ($self->{mergeuploads} && /UP/) {
		    for my $up (keys %$hash) {
			my $h=$hash->{$up};
			if ($h->{SOURCE}{user} eq $m->{SOURCE}{user} &&
			    $h->{SOURCE}{url} eq $m->{SOURCE}{url} &&
			    $h->{hash} eq $m->{hash}) {
			    $m->{''}=$id=$self->{uplinks}{$id}=$up;
			    s/ADD/CHG/;
			    next TRANSFER;
			}
		    }
		}
		$m->{_ctime}=time;
		$hash->{$id}=$m;
	    } elsif (/CHG/) {
		$hash->{$id}={%{$hash->{$id}},%$m};
		my $w=$width->{$id};
		$w=$width->{$id}=[] if !defined $w;
		push @$w,$m->{throughput}*1000/$m->{elapsed} if $m->{elapsed};
		shift @$w if @$w>5;
		my $width=0; $width+=$_ for @$w;
		$width/=@$w if @$w;
		$hash->{$id}{throughput}=$width;
		$hash->{$id}{_daemon}=$daemon;
	    } else {
		if (!$self->{autoclear}) {
		    my $done=0;
		    $done=($hash->{$id}{SOURCE}{transmit}==$hash->{$id}{SOURCE}{total})
			if (exists $hash->{$id}{SOURCE});
		    $hash->{$id}{throughput}=0;
		    $hash->{$id}{completed}=$done;
		    $hash->{$id}{state}=$done?'Complete':'Cancelled'; # ewwwww
		    delete $width->{$id};
		    s/DEL/CHG/;
		}
	    }
	}
	map {$_&&&$_(bless$hash->{$id},'giFT::Transfer')} $self->{handlers}{$c};
	$c=~/DEL/ and delete $hash->{$id};
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
    my ($self)=@_;
    
}

sub bandwidth {
    my ($self)=@_;
    my @width=();
    for my $h ([$self->{upload},"UP"],[$self->{download},"DOWN"]) {
	my $w=0;
	$w+=$_->{throughput}||0 for (values %{$h->[0]});
	push @width,$w;
    }
    return @width;
}

sub uploads {
    my $self=shift;
    values %{$self->{upload}};
}

sub active_uploads {
    my $self=shift;
    grep {!$_->{completed}} values %{$self->{upload}};
}

sub downloads {
    my $self=shift;
    values %{$self->{upload}};
}


package giFT::Transfer;

sub cancel {
    my $self=shift;
    my $cmd={TRANSFER=>
			 {
			     ''=>$self->{''},
			     action=>'cancel'
			 }
	 };
    use Data::Dumper;
    print Dumper $cmd;
    $self->{_daemon}->put($cmd);
}

sub bandwidth {
    $self->{throughput};
}

sub status {
}
