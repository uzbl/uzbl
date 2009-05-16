#!/bin/bash
# you probably really want this in your $XDG_DATA_HOME (eg $HOME/.local/share/uzbl/history)
history_file=/tmp/uzbl.history

# choose from all entries, sorted and uniqued
# goto=`awk '{print $3}' $history_file | sort -u | dmenu -i`


if dmenu --help 2>&1 | grep -q '\[-rs\] \[-ni\] \[-nl\] \[-xs\]'
then
        DMENU="dmenu -i -xs -rs -l 10" # vertical patch
        # choose an item in reverse order, showing also the date and page titles
        goto=`tac $history_file | $DMENU | awk '{print $3}'`
else    
        DMENU="dmenu -i"
	# choose from all entries (no date or title), the first one being current url, and after that all others, sorted and uniqued, in ascending order
	current=`tail -n 1 $history_file | awk '{print $3}'`; goto=`(echo $current; awk '{print $3}' $history_file | grep -v "^$current\$" | sort -u) | $DMENU`
fi 

#[ -n "$goto" ] && echo "cmd uri $goto" > $4
[ -n "$goto" ] && uzblctrl -s $5 -c "cmd uri $goto"
