#!/usr/bin/perl

package Gift::Interface;

use strict;
use warnings;
#use Carp qw(cluck);
#use diagnostics;

use IO::Socket::INET;
use IO::Select;

our(@ISA, $VERSION);
@ISA = qw(Gift);
$VERSION = "1.4";

$| = 1;


my $SELECT = IO::Select->new;
my $STDIN = IO::Handle->new;
my $DAEMON_SOCKET;
my $ID = 100;


my %DebugMessages; # = ("connect" => 1);
my $DebugPretend = 0;

my @DebugLevels =
(
	"\e[1;31m",
	"\e[1,33m",
	"\e[1,37m",
	"\e[1;32m",
	"\e[1,34m"
);

sub new { bless {}, shift }

sub uri_escape
{
	my $self = shift;

	my $tag = shift;
	$tag =~ s/%(..)/pack("c",hex($1))/ge;
	$tag =~ s/\+/ /g;
	$tag =~ s/\_/ /g;
	return $tag;
}

sub uri_parse
{
	my $self = shift;

	my $href = shift;
	my ($protocol,$host,$path,$file,$save);

	if ($href =~ /^(.*?)\:\/\/(.*?)\/(.*)/)
	{
		$protocol = $1;
		$host = $2;
		$path = $3;

		if ($path =~ /.*\/(.*)/)
		{
			$file = $1;
		}
		else
		{
			$file = $path;
		}

		$save = $self->uri_escape($file);
		return ($save, $host, $protocol);
	}
	else
	{
		return 0;
	}
}

sub strip_whitespace
{
	my $self = shift;
	my $data = shift;
	
	$data =~ s/[\r\n\s\t]//g;
	
	return $data;
}

sub send_command
{
	my $self = shift;
	
	my %keys;
	my $command = "\U$_[0]";
	return 0 unless $command;

	my $id = $_[1];

	if ($_[2])
	{
		%keys = %{$_[2]};
	}

	if ($id)
	{
		$self->print_daemon ("$command ($id)\r\n");
	}
	else
	{
		$self->print_daemon ("$command\r\n");
	}
	
	foreach my $key (keys(%keys))
	{
		if ($key && $keys{$key})
		{
			$keys{$key} =~ s/\(/\\\(/g;
			$keys{$key} =~ s/\)/\\\)/g;
			
			$self->print_daemon ("$key ($keys{$key})\r\n");
		}
	}

	$self->print_daemon ("\r\n");
	$self->print_daemon (";\r\n");

	return 1;
}

sub read
{
	my $self = shift;

	my $data = $self->read_daemon ();
	my ($head,%keys) = $self->parse_daemon ($data);

	return ($head,%keys);
}

sub read_single
{
	my $self = shift;

	my $rhead = $_[0];
	my $ritem = $_[1];

	my $data = $self->read_daemon ();
	my ($head,%keys) = $self->parse_daemon ($data);

	if (lc($head) eq lc($rhead) && $keys{$ritem})
	{
		return $keys{$ritem};
	}
	else
	{
		return 0;
	}
}

sub parse_daemon
{
	if (@_ < 2)
	{
		return 0;
	}

	my $self = shift;

	my $scalar_data = shift;
	my (%key_table, $head, $keys, $id, $escaped);

	
	my @read = split (//, $scalar_data);

	my $data;
	my $context;

	my @context;
	my $cur_key;

	# Current context [head,main,value,none]
	my $cx_type = "head";
	my $lastptr = defined;

	foreach my $ptr (@read)
	{
		# Opening specifics
		my $escaped = 0;

		if ($lastptr eq "\\")
		{
			
			$escaped = 1;
		}
		
		if (!$escaped)
		{
			if ($ptr eq "(")
			{
				if ($cx_type eq "head" && defined($data))
				{
					$head = $data;
				}
				
				elsif (defined($data))
				{
					$cur_key = $data;
					$cx_type = "value";
				}
				
				$data = undef;
				next;
			}
			if ($ptr eq "{")
			{
#				print STDOUT "SUB: '$data'\n";
				if (defined($data))
				{
					push (@context, $data)
				}
				
				$cx_type = "main";
				
				$data = undef;
				next;
			}


			# The other side of the scale

			if ($ptr eq ")")
			{
				if ($cx_type eq "head" && defined($data))
				{
					$id = $data;
					$key_table{'id'} = $id;
#					print STDOUT "ID: '$id'\n";
				}
				elsif (defined($data))
				{
					$data =~ s/\\//g;

					if (@context)
					{
#						print STDOUT "Context: @context\n";

						my $ref = \%key_table;
						foreach my $a ( @context )
						{
							$ref = \%{$ref->{lc($a)}};
						}
						$ref->{$cur_key} = $data;
					}

					else
					{
						$key_table{$cur_key} = $data;
					}
				}

				$cx_type = "main";				
				$data = undef;
				next;
			}
			if ($ptr eq "}")
			{
				$cx_type = "main";
				
				$data = undef;
				
				pop (@context);
				next;
			}

			if ($ptr eq ";")
			{
				$cx_type = "none";
				
				$data = undef;

				last;
			}

			if ($ptr =~ /^[\s\t\n]$/ && $cx_type ne "value")
			{
				if ($cx_type eq "head" && defined($data))
				{
					$head = $data;
					$cx_type = "main";
					$data = undef;
				}
				elsif ($cx_type eq "main" && defined($data))
				{
					$cur_key = $data;
				}

				next;
			}
		}

		$data .= $ptr;
		
		$lastptr = $ptr;
		$ptr = undef;
	}
	
	if ($head)
	{
#		print STDOUT "HEAD: '$head'\n";
#		dump_hash (\%key_table);
	
		return ($head,%key_table);
	}
	else { return 0 }
}

sub connect
{
	my $self = shift;
	my $addr = shift;
	my $port = shift;
	
	$addr ||= "127.0.0.1";
	$port ||= 1213;

	$self->debug
	(
		"connect", 2,
		"Connecting to giFT daemon [$addr:$port]\n"
	);

	# while (!$DAEMON_SOCKET)
	if (!$DAEMON_SOCKET)
	{
		$DAEMON_SOCKET = new IO::Socket::INET (
				PeerAddr => $addr,
				PeerPort => $port,
				Proto => 'tcp'
		);
	}

	if ($DAEMON_SOCKET)
	{
		$self->debug
		(
			"connect", 2,
			"Connected successfully.\n"
		);
	}
	else
	{
		$self->debug
		(
			"connect", 0,
			"Connection to giFT daemon failed.\n"
		);
		return 0;
	}

	$DAEMON_SOCKET->autoflush(1);
	$SELECT->add($DAEMON_SOCKET);

	return 1;
}

sub disconnect
{
	my $self = shift;
	
	$SELECT->remove($DAEMON_SOCKET);
	$DAEMON_SOCKET->close;
	
	$DAEMON_SOCKET = undef;
}

sub can_read
{
	my $self = shift;
	
	return $DAEMON_SOCKET;
}

sub select_stdin
{
	my $self = shift;

	$STDIN->fdopen(fileno(STDIN),"r") or return 0;
	$STDIN->autoflush(1);
	$SELECT->add($STDIN);

	return 1;
}

sub print_daemon
{
	my $self = shift;
	
	$self->debug ("print_daemon", 4, "@_");
	
	return 0 unless $DAEMON_SOCKET;
	$DAEMON_SOCKET->print("@_") if (!$DebugPretend);
}

sub read_stdin
{
	my $self = shift;

	foreach my $rh ($SELECT->can_read(1))
	{
		if ($rh == $STDIN)
		{
			if (my $stdin_data = <$STDIN>)
			{
				# Do something with $stdin_data
				return $stdin_data;
			}
			else
			{
				$SELECT->remove($STDIN);
				close($STDIN);

				return 0;
			}
		}
	}
}

sub read_daemon
{
	my $self = shift;

	foreach my $rh ($SELECT->can_read(1))
	{
		# If it is a socket handle
		if ($rh == $DAEMON_SOCKET)
		{
			if (my $socket_data = <$rh>)
			{
				# Do something with $socket_data
				$self->debug ("read_daemon", 4, $socket_data);
				return $socket_data;
			}
			else
			{
				$self->disconnect;
				return 0;
			}
		}

	}
}


##########################
# Special giFT functions #


sub attach
{
	my $self = shift;

	my $client = $_[0];
	my $version = $_[1];

	$self->send_command ("attach", "",
		{
			"client" => $client,
			"version" => $version
		}
	);
}

sub search
{
	my $self = shift;

	my ($realm, $query, $exclude, $protocol) = @_;
	$realm ||= "everything"; $exclude ||= ""; $protocol ||="";

	return 0 unless $query;
	
	$ID++;
	
	$self->debug ("search", 3,
						"\tStarting new search\n",
						"\t\trealm => $realm,\n",
						"\t\tquery => $query,\n",
						"\t\texclude => $exclude,\n",
						"\t\tprotocol => $protocol\n");
	
	$self->send_command ("search", $ID,
		{
			"realm" => $realm,
			"query" => $query,
			"exclude" => $exclude,
			"protocol" => $protocol
		}
	);
}

sub locate
{
	my $self = shift;
	
	my $query = shift;
	
	$ID++;

	$self->debug ("locate", 3,
						"\tLocating file\n",
						"\t\tquery => $query,\n",);
	
	$self->send_command ("locate", $ID,
		{
			"query" => $query
		}
	);
}

sub download
{
	my $self = shift;

	my ($url, $hash, $size, $save, $user, $id) = @_;
	
	if ( not ($url && $hash && $size && $save && $user) )
	{
			return 0;
	}
	
	$ID++;
	$id ||= $ID;
	
	$self->debug ("download", 3,
						"\tStarting download ($id)\n",
						"\t\tuser => $user,\n",
						"\t\thash => $hash,\n",
						"\t\turl => $url,\n",
						"\t\tsize => $size,\n",
						"\t\tsave => $save\n");
						
	
	$self->send_command ("addsource", $id,
		{
			"user" => $user,
			"hash" => $hash,
			"url" => $url,
			"size" => $size,
			"save" => $save
		}
	);
		
	return $id;
}

sub transfer
{
	my $self = shift;
	
	my $id = shift;
	my $action = shift;
	
	return 0 unless ($id && $action =~ /^(pause|resume|cancel)$/i);
	
	$self->debug ("transfer", 3, "( id => $id, action => $action )\n");
	
	$self->send_command ("transfer", $id,
			{
				"action" => $action
			}
	);
}

sub stats
{
	my $self = shift;
	my $protocol = shift;

	$self->send_command ("stats", { "protocol" => $protocol });
}

sub share
{
	my $self = shift;
	my $action = shift;
	
	return 0 unless ($action =~ /^(sync|hide|show)$/i);
	
	$self->debug ("share", 3, "( action => $action )\n");
	
	$self->send_command ("share", "",
		{
			"action" => $action
		}
	);
}

sub quit
{
	my $self = shift;

	$self->send_command ("quit");
}

sub detach
{
	my $self = shift;

	$self->send_command ("detach");
}




##################
# DEBUGGING ONLY #

sub debugmessage
{
	my $self = shift;
	my @messages = @_;
	
	foreach my $debugmsg (@messages)
	{
		if ($debugmsg =~ /^\-(.*)/)
		{
			$DebugMessages{$1} = -1;
		}
		elsif ($debugmsg =~ /^\+(.*)/)
		{
			$DebugMessages{$1} = 1;
		}
	}
}

sub debug
{
	my $self = shift;
	
	my $package = shift;
	my $level = shift;
	my @data = @_;

	my $nlevel = $DebugLevels[$level];
	
	return 0 if (defined $DebugMessages{'all'} && $DebugMessages{'all'} < 0);	
	
	if ($DebugMessages{$package} || $DebugMessages{'all'})
	{
		if (!defined $DebugMessages{$package} || $DebugMessages{$package} >= 0)
		{
			foreach my $data (@data)
			{
				print STDERR "[${nlevel}Gift->${package}\e[m] $data";
				#print STDERR "[($$) ${nlevel}Gift->${package}\e[m] $data";
			}
		}
	#cluck;
	}
}

sub pretend
{
	my $self = shift;

	$DebugPretend = shift;
}

sub dump_hash
{
	my $hashref = shift;
	foreach my $key ( keys %{$hashref} )
	{
		if ( ref ($hashref->{$key} ) )
		{
			print STDOUT "!! '$key' !!\n";
			dump_hash($hashref->{$key});
		}
		else
		{
			print STDOUT "'$key' = '".$hashref->{$key}."'\n";
		}
	}
}

return 1;
