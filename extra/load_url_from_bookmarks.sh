#!/bin/bash
file=bookmarks
goto=`awk '{print $1}' $history_file | dmenu` #NOTE: it's the job of the script that inserts bookmarks to make sure there are no dupes.
[ -n "$goto" ] && echo "uri $goto" > /tmp/uzbl-fifo-name-TODO
