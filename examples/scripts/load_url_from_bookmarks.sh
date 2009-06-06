#!/bin/bash

#NOTE: it's the job of the script that inserts bookmarks to make sure there are no dupes.

[ -f /usr/share/uzbl/examples/data/bookmarks ] && file=/usr/share/uzbl/examples/data/bookmarks
[ -f $XDG_DATA_HOME/uzbl/bookmarks           ] && file=$XDG_DATA_HOME/uzbl/bookmarks
[ -f ./examples/data/bookmarks               ] && file=./examples/data/bookmarks #useful when developing
[ -z "$file" ] && exit 1
COLORS=" -nb #303030 -nf khaki -sb #CCFFAA -sf #303030"
if dmenu --help 2>&1 | grep -q '\[-rs\] \[-ni\] \[-nl\] \[-xs\]'
then
	DMENU="dmenu -i -xs -rs -l 10" # vertical patch
	# show tags as well
	goto=`$DMENU $COLORS < $file | awk '{print $1}'`
else
	DMENU="dmenu -i"
	# because they are all after each other, just show the url, not their tags.
	goto=`awk '{print $1}' $file | $DMENU $COLORS`
fi

#[ -n "$goto" ] && echo "uri $goto" > $4
[ -n "$goto" ] && uzblctrl -s $5 -c "uri $goto"
