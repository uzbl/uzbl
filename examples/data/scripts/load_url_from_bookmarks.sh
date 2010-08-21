#!/bin/bash

#NOTE: it's the job of the script that inserts bookmarks to make sure there are no dupes.

source $UZBL_UTIL_DIR/uzbl-args.sh
source $UZBL_UTIL_DIR/uzbl-dir.sh

[ -r "$UZBL_BOOKMARKS_FILE" ] || exit 1

COLORS=" -nb #303030 -nf khaki -sb #CCFFAA -sf #303030"
if dmenu --help 2>&1 | grep -q '\[-rs\] \[-ni\] \[-nl\] \[-xs\]'; then
	DMENU="dmenu -i -xs -rs -l 10" # vertical patch
	# show tags as well
	goto=$($DMENU $COLORS < $UZBL_BOOKMARKS_FILE | awk '{print $1}')
else
	DMENU="dmenu -i"
	# because they are all after each other, just show the url, not their tags.
	goto=$(awk '{print $1}' $UZBL_BOOKMARKS_FILE | $DMENU $COLORS)
fi

#[ -n "$goto" ] && echo "uri $goto" > $UZBL_FIFO
[ -n "$goto" ] && echo "uri $goto" | socat - unix-connect:$UZBL_SOCKET
