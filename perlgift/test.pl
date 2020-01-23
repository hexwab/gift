#!/usr/bin/perl -w

use giFT::daemon;

use Data::Dumper;

my $d=new giFT::daemon;

$q=shift or die 'no search query';

$d->put({search=>{query=>$q}});

while (1) {
    while (my $m=$d->get) {
    print Dumper $m if defined $m;
}
    sleep(1);

}
