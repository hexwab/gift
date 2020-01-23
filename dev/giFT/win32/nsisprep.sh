#!/bin/sh
# $Id: nsisprep.sh,v 1.8 2002/10/24 07:06:06 rossta Exp $

cd ~/src/gift/win32

mkdir -p tmp

for file in AUTHORS COPYING NEWS
do
	perl -p -e 's/\n/\r\n/;' ../${file} >tmp/${file}
done

perl -p -e 's/\n/\r\n/;' ../doc/README >tmp/README

perl -p -e 's/\n/\r\n/; s/^\s*plugins\s*=\s*libOpenFT\.so/plugins = OpenFT\.dll/; s/incoming\s+=\s+~\/\.giFT\/incoming/incoming = \/C\/Program Files\/giFT\/incoming/; s/completed\s+=\s+~\/\.giFT\/completed/completed = \/C\/Program Files\/giFT\/completed/; ' ../etc/gift.conf >tmp/gift.conf

perl -p -e 's/\n/\r\n/; s/^\s*port\s*=.*/port=0/; s/^\s*http_port\s*=.*/http_port=0\r\nfirewalled=0/;' ../etc/OpenFT/OpenFT.conf >tmp/OpenFT.conf
