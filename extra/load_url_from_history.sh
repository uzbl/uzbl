#!/bin/bash
history_file=/tmp/uzbl.history
goto=`awk '{print $3}' $history_file | sort | uniq | dmenu`
[ -n "$goto" ] && echo "uri $goto" > $4
