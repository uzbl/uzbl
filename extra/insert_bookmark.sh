#!/bin/bash
file=bookmarks

which zenity &>/dev/null || exit 2

entry=`zenity --entry --text="Add bookmark. add tags at the end, separated by commas" --entry-text="$5"`
url=`awk '{print $1}' <<< $entry`
# TODO: check if already exists, if so, and tags are different: ask if you want to replace tags
echo "$entry" >> $file
