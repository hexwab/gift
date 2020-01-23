#!/usr/bin/perl

my $filename  = shift;
my $transmit  = shift;
my $total     = shift;
my $incoming  = shift || "$ENV{HOME}/.giFT/incoming";
my $completed = shift || "$ENV{HOME}/.giFT/completed";

print "\n";
print "filename  = $filename\n";
print "transmit  = $transmit\n";
print "total     = $total\n";
print "incoming  = $incoming\n";
print "completed = $completed\n";
print "\n";

my $a_player     = "xmms";
my $a_player_opt = "--enqueue";

if ($a_player && $filename =~ /\.(mp3|ogg|wav)$/)
{
	# check to see if xmms has an active pid before trying to enqueue
	my $a_player_pid = `pidof $a_player`;
	chomp $a_player_pid;

	execute ("$a_player $a_player_opt \"$filename\"") if ($a_player_pid);
}

if ($filename=~/\.jpe?g/i) {
    system('mv','-vi',$filename,'/home/HEx/dgeln/images/gift');
}

###############################################################################

sub execute 
{
	my $cmd = shift;
	
	print "executing $cmd...\n";
	system ($cmd);
}
