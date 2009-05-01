#!/bin/bash
# you probably really want this in your $XDG_DATA_HOME (eg $HOME/.local/share/uzbl/history)
history_file=/tmp/uzbl.history

# choose from all entries, sorted and uniqued
# goto=`awk '{print $3}' $history_file | sort -u | dmenu -i`

# choose from all entries, the first one being current url, and after that all others, sorted and uniqued.
current=`tail -n 1 $history_file | awk '{print $3}'`; goto=`(echo $current; awk '{print $3}' $history_file | grep -v "^$current\$" | sort -u) | dmenu -i`
#[ -n "$goto" ] && echo "uri $goto" > $4
[ -n "$goto" ] && uzblctrl -s $5 -c "uri $goto"
