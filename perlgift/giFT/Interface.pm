#!/usr/bin/perl -w

package giFT::Interface;
require Exporter;
use strict;

use vars qw[@ISA @EXPORT_OK];

@ISA=qw[Exporter];
@EXPORT_OK=qw[serialize unserialize unserialize_all];

sub serialize(@) {
    my $out='';
    for my $this (@_) {
	$out.=_serialize($this,1).";\n";
    }
    $out;
}

sub _serialize {
    my $out='';
    my $h='';
    my ($this,$hack)=@_;
    if (ref($this) eq 'HASH' || $hack) {
	for my $key (keys %$this) {
	    my $vals=$this->{$key};
	    if (!$key && !$hack) {
		($h=$vals)=~s/([]\[(){};\\])/\\$1/g;
		next;
	    }

	    for my $val ((ref($vals) eq 'ARRAY')?@$vals:$vals) {
		$out.="$key ";
		if (ref($val)) {
		    my ($tmp,$h)=_serialize($val,0);
		    if ($hack){
			$out.=$h.$tmp;
		    } else {
			$out.=$h."{$tmp}";
		    }
		} elsif (defined $val) {
		    $val=~s/([]\[(){};\\])/\\$1/g;
		    $out.="($val)";
		}
	    }
	}
    }
    wantarray?($out,"($h)"):$out;
}

sub _unserialize_one {
    my ($out,$command,$val);
    local $@;

    eval {
	$command=_key();
	$val=_value();
    
	$out=_unserialize();
	$out->{''}=$val if defined $val;
    };
    die "Parse error: '".substr($_,pos)."' $_" if $@ && pos!=length;
    return undef if $@;
    (bless{$command=>$out},'giFT::Interface::Command');
#    ({$command=>$out});
}

sub unserialize($) {
    local $_=shift;

    wantarray?(_unserialize_one,substr($_,pos)):_unserialize_one;
}    

sub unserialize_all($) {
    local $_=shift;
    my @results;
    while (my $r=_unserialize_one) {
	push @results,$r;
    }

    @results and return (\@results,substr($_,pos));
    (undef,$_);
}

sub _key {
    # be robust in spite of what the spec says
#    /\G\s*(\w[\d\w-]*)\s*/gcs or die "Parse error";
    /\G((\\.|[^ (;}])*)\s*/gc or die "Parse error";

    my $key=$1;

    $key=~s/\\(.)/$1/g;
    $key;
}

sub _value {
    my $val;

    if (/\G\(/gc) {
	/\G((\\.|[^])}])*).\s*/gc or die "Parse error";

	($val=$1)=~s/\\(.)/$1/g;
    }
    $val;
}

sub _unserialize {
    my %out=();

    while (!/\G\s*[;}]\s*/gc) {
	my ($key,$val,$sub);
	$key=_key();
	
	$sub=_unserialize() if (/\G\{\s*/gc);
	if (defined (my $tmp=_value())) {
	    $val=$tmp;
	    $sub=_unserialize() if (/\G\{\s*/gc);
	}
	
	my $temp;
	if (defined $sub) {
	    $sub->{''}=$val if (defined $val);
	    $temp=$sub;
	} else {
	    $temp=$val;
	}

	if (!exists $out{$key}) {
	    $out{$key}=$temp;
	} elsif (ref($out{$key}) ne 'ARRAY') {
	    $out{$key}=[$out{$key},$temp];
	} else {
	    push @{$out{$key}},$temp;
	}


    }

    return \%out;
}

1;

package giFT::Interface::Command;

sub new {
    bless $_[1];
}

sub value {
    my ($foo,$val)=@_;
    
    return $val if !ref($val); # common case
    my @val=($val);
    
    {
	my $i;
	@val=map {
	    (ref $_)?do {
		$i++;
		(ref)eq'ARRAY'?@$_:$_->{''}
	    }:$_
	    } @val;
	redo if $i;
    }

    @val;
}

1;

__END__
=head1 NAME

giFT::Interface - giFT interface protocol

=head1 SYNOPSIS

  use giFT::Interface qw(serialize unserialize);

  my $hash={ search=>{ query=>"test" } });

  my $string=serialize($hash);

  # string now contains "search query (test);\n"

  my $newhash=unserialize($string);
 
  # $newhash is the same as the original $hash

=head1 DESCRIPTION

Parses giFT's interface protocol into a more usable form.

=head1 TERMINOLOGY

The semicolon-terminated strings that comprise the interface protocol
are referred to here as "commands", despite the fact that often they
aren't actually commands.

=head1 FUNCTIONS

=over 4

=item B<($hash, $rest) = unserialize ($string)>

Takes a string from giFT and returns a B<giFT::Interface::Command>
object, and the remainder of the string.  Returns undef if the string
does not contain a complete command. Dies if it encounters a syntax
error (this shouldn't happen; it would indicate a bug in giFT).

=item B<($hasharray, $rest) = unserialize_all ($string)>

Returns all of the available commands in the string as an array of
B<giFT::Interface::Command>s. Returns an empty array if no commands are
parsable. See unserialize for more details.

Depending on the size of the input string, this may be much more
efficient than unserialize().

=item B<serialize ($hash)>

Does the opposite of unserialize, i.e. returns a string corresponding
to the given hash reference(s). Blessing the reference into
B<giFT::Interface::Command> is permissible but not required.

=head1 COMMAND OBJECTS

These are just blessed hash references; the contents are intended to
be accessed directly.

Item keys and values are stored as hash keys and values. Multiple
values for a single key are stored as an array reference. Subitems
have a hash reference instead; the value, if any, is stored in the key
'' (null string) within the subhash.

Example: calling unserialize() on the string

"COMMAND(number) ITEM(value) SUBCOMMAND(subvalue) { SUBITEM } FOO(1) FOO(2);"

would return a giFT::Interface::Command:
        {
          'COMMAND' => {
                         '' => 'number',
                         'ITEM' => 'value',
                         'SUBCOMMAND' => {
                                           '' => 'subvalue',
                                           'SUBITEM' => undef
                                         },
                         'FOO' => [
                                    '1',
                                    '2'
                                  ]
                       }
        }

There is one convenience method provided:

=item B<value ($value)>

Takes a hash value, ignores all the subcommands and returns a list of
all the values. This correctly handles multiple values that may
contain subcommands.

Example: passing the value

        [
          '1',
          {
            '' => '2',
            'a' => undef
          },
          undef
        ]

will return ('1', '2', undef).

=head1 BUGS

unserialize() is very slow at processing large strings. Use
unserialize_all() instead, or call unserialize() in scalar context if
you don't need the unparsed part of the string.

Parsing is rather permissive; many syntactically-incorrect input
strings will simply produce odd results rather than throwing an error.

No attempt at pretty-printing is done; output strings have ugly,
inconsistent spacing.

=head1 SEE ALSO

L<giFT::Daemon>, L<giFT(1)>.

=head1 AUTHOR

Tom Hargreaves E<lt>HEx@freezone.co.ukE<gt>

=head1 LICENSE

Copyright 2003 by Tom Hargreaves.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
