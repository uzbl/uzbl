#!/bin/sh

[ -d "${XDG_DATA_HOME:-$HOME/.local/share}/uzbl" ] || exit 1
file=${XDG_DATA_HOME:-$HOME/.local/share}/uzbl/bookmarks

which zenity &>/dev/null || exit 2
url=$6
# replace tabs, they are pointless in titles and we want to use tabs as delimiter.
title=$(echo "$7" | sed 's/\t/    /')
entry=`zenity --entry --text="Add bookmark. add tags after the '\t', separated by spaces" --entry-text="$url $title\t"`
exitstatus=$?
if [ $exitstatus -ne 0 ]; then exit $exitstatus; fi
url=`echo $entry | awk '{print $1}'`

# TODO: check if already exists, if so, and tags are different: ask if you want to replace tags
echo "$entry" >/dev/null #for some reason we need this.. don't ask me why
echo -e "$entry"  >> $file
true
