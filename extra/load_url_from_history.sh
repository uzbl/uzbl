#!/bin/bash
history_file=/tmp/uzbl.history
goto=`awk '{print $3}' $history_file | dmenu`
[ -n "$goto" ] && echo "uri $goto" > /tmp/uzbl-fifo-name-TODO
