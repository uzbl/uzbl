#!/bin/bash
# you probably really want this in your $XDG_DATA_HOME (eg $HOME/.local/share/uzbl/history)
history_file=/tmp/uzbl.history

goto=`awk '{print $3}' $history_file | sort | uniq | dmenu -i`
[ -n "$goto" ] && echo "uri $goto" > $4
