#!/bin/sh
# $Id: nsisprep.sh,v 1.6 2002/05/12 01:01:05 rossta Exp $

mkdir -p tmp

for file in AUTHORS COPYING NEWS README 
do
	perl -p -e 's/\n/\r\n/;' ../${file} >tmp/${file}
done

perl -p -e 's/\n/\r\n/; s/^\s*plugins\s*=\s*libOpenFT\.so/plugins = OpenFT\.dll/;' ../etc/gift.conf >tmp/gift.conf
perl -p -e 's/\n/\r\n/; s/^\s*port\s*=.*/port=0/; s/^\s*http_port\s*=.*/http_port=0\r\nfirewalled=0/;' ../etc/OpenFT/OpenFT.conf >tmp/OpenFT.conf
perl -p -e 's/\n/\r\n/;' ../etc/ui/ui.conf >tmp/ui.conf
