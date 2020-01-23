#!/usr/bin/perl

my @nodes = ();
my $cver = 2050;

while (<>)
{
	my ($vit, $up, $ip, $pt, $hpt, $cls, $ver) = split;

	next unless ($vit && $up && $ip && $pt && $hpt && $cls >= 3 && $ver == $cver);

	my %node = ('vit' => $vit,
				'up' => $up,
				'ip' => $ip,
				'pt' => $pt,
				'hpt' => $hpt,
				'cls' => $cls,
				'ver' => $ver);

	push @nodes, \%node;
}

print "1500000000 10000000 68.116.102.125 1215 1216 3 $cver\n";

@nodes = sort { $$b{'up'} <=> $$a{'up'} } @nodes;
foreach my $ref (@nodes)
{
	my %n = %$ref;
	print "$n{vit} $n{up} $n{ip} $n{pt} $n{hpt} $n{cls} $n{ver}\n";
}
