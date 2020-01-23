perl -MgiFT::Interface=unserialize -e 'undef $/;$a=<>;while (($h,$a)=unserialize $a) {}'
