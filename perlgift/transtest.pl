#!/usr/bin/perl -w
use giFT::Daemon;
use giFT::TransferHandler;

# create a new blocking daemon connection
my $daemon=new giFT::Daemon(undef,undef,1,0);

my $th=new giFT::TransferHandler($daemon,
				 \&addup,
				 \&chgup,
				 \&delup,
				 \&adddown,
				 \&chgdown,
				 \&deldown
				 );
    
$daemon->attach;

while (1) {
    $daemon->poll;
    if (!(time %10)) {
	use Data::Dumper;
	print Dumper $th;
	sleep 1;
    }
}

sub addup {
    print "addup\n";
}

sub chgup {
}

sub delup {
}

sub adddown {
}
sub chgdown {
}
sub deldown {
}
