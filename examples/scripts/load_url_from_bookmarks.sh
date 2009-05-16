#!/bin/bash

if dmenu --help 2>&1 | grep -q '\[-rs\] \[-ni\] \[-nl\] \[-xs\]'
then
	DMENU="dmenu -i -xs -rs -l 10" # vertical patch
else
	DMENU="dmenu -i"
fi
# you probably want your bookmarks file in your $XDG_DATA_HOME ( eg $HOME/.local/share/uzbl/bookmarks)
if [ -f /usr/share/uzbl/examples/data/bookmarks ]
then
        file=/usr/share/uzbl/examples/data/bookmarks
else
        file=./examples/data/bookmarks #useful when developing
fi

goto=`awk '{print $1}' $file | $DMENU` #NOTE: it's the job of the script that inserts bookmarks to make sure there are no dupes.
#[ -n "$goto" ] && echo "uri $goto" > $4
[ -n "$goto" ] && uzblctrl -s $5 -c "uri $goto"
