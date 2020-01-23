package Site::DBInfo;

sub new 
{
	my $info = 
	{
		'dsn' => 'DBI:mysql:database=gift;host=localhost',
		'user' => 'root',
		'pass' => ''
	};

	return $info;
};

1;
