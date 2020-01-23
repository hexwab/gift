#!/usr/bin/perl

my @nodes = ();

while (<>)
{
	my ($vit, $up, $ip, $pt, $hpt, $cls, $ver) = split;

	next unless ($vit && $up && $ip && $pt && $hpt && $cls >= 3 && $ver == 1800);

	my %node = ('vit' => $vit,
				'up' => $up,
				'ip' => $ip,
				'pt' => $pt,
				'hpt' => $hpt,
				'cls' => $cls,
				'ver' => $ver);

	push @nodes, \%node;
}

print "1500000000 10000000 68.116.105.76 1215 1216 3 1800\n";

@nodes = sort { $$b{'up'} <=> $$a{'up'} } @nodes;
foreach my $ref (@nodes)
{
	my %n = %$ref;
	print "$n{vit} $n{up} $n{ip} $n{pt} $n{hpt} $n{cls} $n{ver}\n";
}
