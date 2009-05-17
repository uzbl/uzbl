#!/bin/bash
# you probably want your bookmarks file in your $XDG_DATA_HOME ( eg $HOME/.local/share/uzbl/bookmarks)


[ -f /usr/share/uzbl/examples/data/bookmarks ] && file=/usr/share/uzbl/examples/data/bookmarks  # you will probably get permission denied errors here.
[ -f $XDG_DATA_HOME/uzbl/bookmarks           ] && file=$XDG_DATA_HOME/uzbl/bookmarks
[ -f ./examples/data/bookmarks               ] && file=./examples/data/bookmarks #useful when developing
[ -z "$file" ] && exit 1

which zenity &>/dev/null || exit 2

entry=`zenity --entry --text="Add bookmark. add tags after the '\t', separated by spaces" --entry-text="$6 $7\t"`
url=`awk '{print $1}' <<< $entry`
# TODO: check if already exists, if so, and tags are different: ask if you want to replace tags
echo "$entry" >/dev/null #for some reason we need this.. don't ask me why
echo -e "$entry"  >> $file
true