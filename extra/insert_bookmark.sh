#!/bin/bash
# $1 should be the uri you want to bookmark
file=bookmarks

[ -z "$1" ] && exit 1
which zenity &>/dev/null || exit 2

entry=`zenity --entry --text="Add bookmark. add tags at the end, separated by commas" --entry-text="$1"`
url=`awk '{print $1}' <<< $entry`
# TODO: check if already exists, if so, and tags are different: ask if you want to replace tags
echo "$entry" >> $file
