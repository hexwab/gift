#!/bin/sh
# $Id: nsisprep.sh,v 1.13 2003/05/16 16:02:39 rossta Exp $

mkdir -p tmp

UNAME=`uname | cut -c 1-6`
if [ $UNAME = CYGWIN ]; then
NL=\\n
else
NL=\\r\\n
fi

for file in AUTHORS COPYING NEWS
do
	perl -p -e "s/\n/${NL}/;" ../${file} >tmp/${file}
done

perl -p -e "s/\n/${NL}/;" ../doc/README >tmp/README

perl -p -e "s/\n/${NL}/; s/^\s*plugins\s*=\s*libOpenFT\.so/plugins = OpenFT\.dll/; s/incoming\s+=\s+~\/\.giFT\/incoming/incoming = \/C\/Program Files\/giFT\/incoming/; s/completed\s+=\s+~\/\.giFT\/completed/completed = \/C\/Program Files\/giFT\/completed/; " ../etc/gift.conf >tmp/gift.conf

perl -p -e "s/\n/${NL}/; s/^\s*port\s*=.*/port=0/; s/^\s*http_port\s*=.*/http_port=0\nfirewalled=0/;" ../etc/OpenFT/OpenFT.conf >tmp/OpenFT.conf

perl -p -e "s/\n/${NL}/;" ../data/Gnutella/Gnutella.conf >tmp/Gnutella.conf

if [ -e ../FastTrack/data/nodes ]; then
	perl -p -e "s/\n/${NL}/;" ../FastTrack/data/nodes >tmp/nodes
fi
