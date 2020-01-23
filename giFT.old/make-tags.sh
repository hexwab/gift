#!/bin/sh

ctags `find ~/c/giFT/ -name "*.[ch]" | grep -v /lib/`
mv tags ~/c/giFT/tags
