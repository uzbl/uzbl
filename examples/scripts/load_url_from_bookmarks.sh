#!/bin/bash

# you probably want your bookmarks file in your $XDG_DATA_HOME ( eg $HOME/.local/share/uzbl/bookmarks)
if [ -f /usr/share/uzbl/examples/bookmarks ]
then
        file=/usr/share/uzbl/examples/bookmarks
else
        file=./examples/bookmarks #useful when developing
fi

goto=`awk '{print $1}' $file | dmenu -i` #NOTE: it's the job of the script that inserts bookmarks to make sure there are no dupes.
#[ -n "$goto" ] && echo "uri $goto" > $4
[ -n "$goto" ] && uzblctrl -s $5 -c "uri $goto"
