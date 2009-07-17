#!/bin/sh
#TODO: strip 'http://' part

file=${XDG_DATA_HOME:-$HOME/.local/share}/uzbl/history
[ -d `dirname $file` ] || exit 1
echo "$8 $6 $7" >> $file
