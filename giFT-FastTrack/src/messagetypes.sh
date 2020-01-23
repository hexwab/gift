perl -MData::Dumper -ne '/message type (.*?),/ && $a{$1}++;END{print "$_ => $a{$_}\n" for sort keys %a}'
