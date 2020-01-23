#!/bin/sh
# $Id: nsisprep.sh,v 1.3 2002/04/30 13:56:51 rossta Exp $

mkdir -p tmp

for file in AUTHORS COPYING NEWS README 
do
	perl -p -e 's/\n/\r\n/;' ../${file} >tmp/${file}
done

perl -p -e 's/\n/\r\n/; s/^\s*plugins\s*=\s*libOpenFT\.so/#plugins = OpenFT\.dll/;' ../etc/gift.conf >tmp/gift.conf
perl -p -e 's/\n/\r\n/;' ../etc/OpenFT/OpenFT.conf >tmp/OpenFT.conf
perl -p -e 's/\n/\r\n/;' ../etc/ui/ui.conf >tmp/ui.conf
