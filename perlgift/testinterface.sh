perl -MgiFT::Interface=unserialize -MData::Dumper -e 'undef $/;$a=<>;while (($h,$a)=unserialize $a) {print Dumper($h)."\n"}'
