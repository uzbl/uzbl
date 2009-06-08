#!/bin/bash
# you probably want your bookmarks file in your $XDG_DATA_HOME ( eg $HOME/.local/share/uzbl/bookmarks)

[ -d "$XDG_DATA_HOME/uzbl" ] || exit 1
file=$XDG_DATA_HOME/uzbl/bookmarks

which zenity &>/dev/null || exit 2

entry=`zenity --entry --text="Add bookmark. add tags after the '\t', separated by spaces" --entry-text="$6 $7\t"`
url=`awk '{print $1}' <<< $entry`
# TODO: check if already exists, if so, and tags are different: ask if you want to replace tags
echo "$entry" >/dev/null #for some reason we need this.. don't ask me why
echo -e "$entry"  >> $file
true
