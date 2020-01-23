use giFT::Daemon;
use giFT::Shares;
use giFT::Interface;
use Data::Dumper;
use Time::HiRes qw[gettimeofday tv_interval];
my $match=shift;

my $time;

my $daemon=new giFT::Daemon(undef,undef,1,undef);

$daemon->attach;

my $shares=new giFT::Shares($daemon);

$time=[gettimeofday];
do {
    $daemon->poll;
} while (!$shares->share_status);
print tv_interval($time,[gettimeofday])."\n";

$time=[gettimeofday];
print ($shares->shares." shares\n");
for ($shares->shares) {
    my $a=giFT::Interface::serialize({item=>$_});
}
print tv_interval($time,[gettimeofday])."\n";
#for (map {Dumper $_}($shares->shares)) {
#    /$match/i and print;
#}
