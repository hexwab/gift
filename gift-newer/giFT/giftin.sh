#!/bin/sh
# $Id: giftin.sh,v 1.2 2002/04/17 00:02:48 jasta Exp $

# NOTE: this isn't exactly how I like it, but it's close enough so that if
# you're going to completely ignore my formatting style you had damn well 
# better run this before you send patches

indent -bad \
       -bl \
       -bli0 \
       -brsn \
       -cli1 \
       -cp1 \
       -hnl \
       -kr \
       -l80 \
       -lps \
       -ncs \
       -nfca \
       -pcs \
       -sob \
       -ts4 \
       $@
