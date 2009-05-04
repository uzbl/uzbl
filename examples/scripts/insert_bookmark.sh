#!/bin/bash
# you probably want your bookmarks file in your $XDG_DATA_HOME ( eg $HOME/.local/share/uzbl/bookmarks)

if [ -f /usr/share/uzbl/examples/data/bookmarks ]
then
	file=/usr/share/uzbl/examples/data/bookmarks # you will probably get permission denied errors here. pick a file in your ~
else
	file=./examples/bookmarks #useful when developing
fi

which zenity &>/dev/null || exit 2

entry=`zenity --entry --text="Add bookmark. add tags at the end, separated by commas" --entry-text="$6"`
url=`awk '{print $1}' <<< $entry`
# TODO: check if already exists, if so, and tags are different: ask if you want to replace tags
echo "$entry" >> $file
