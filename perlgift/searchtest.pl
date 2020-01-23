#!/usr/bin/perl -w
use giFT::Daemon;
use giFT::Search;

# create a new blocking daemon connection
my $daemon=new giFT::Daemon(undef,undef,1,1);

$daemon->attach; # giFT bug workaround

my $search=new giFT::Search($daemon,
			    search=>shift||'linux avi',
			    \&search_result,
			      \&search_done);

do {
    $daemon->poll;
} while (!$search->finished);

my $results=$search->results;
my $unique=$search->unique_results;

print "Got $results results ($unique unique)\n";

sub search_result {
    my ($search, $result)=@_;
    print "Search result: $result->{file}\n";
#    print (scalar($search->find_results_by_hash($result->{hash}))." sources so far\n");
}

sub search_done {
    my ($search)=@_;
    print "Search finished\n";
}
