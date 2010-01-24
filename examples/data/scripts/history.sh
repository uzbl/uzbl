#!/bin/sh

file=${XDG_DATA_HOME:-$HOME/.local/share}/uzbl/history
[ -d `dirname $file` ] || exit 1
echo `date +'%Y-%m-%d %H:%M:%S'`" $6 $7" >> $file
