#!/bin/sh
# $Id: debug_setup.sh,v 1.1 2002/03/25 06:30:12 rossta Exp $

mkdir Debug
cd Debug
cp ../../data\mime.types .
mkdir OpenFT
cd OpenFT
cp ../../../data/OpenFT/nodes 
cp ../../../data/OpenFT/*.gif 
cp ../../../data/OpenFT/*.jpg 
cp ../../../data/OpenFT/*.css .
