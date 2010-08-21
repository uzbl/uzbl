#!/bin/sh

#NOTE: it's the job of the script that inserts bookmarks to make sure there are no dupes.

source $UZBL_UTIL_DIR/dmenu.sh
source $UZBL_UTIL_DIR/uzbl-args.sh
source $UZBL_UTIL_DIR/uzbl-dir.sh

[ -r "$UZBL_BOOKMARKS_FILE" ] || exit 1

if [ "x$DMENU_HAS_VERTICAL" = "x" ]; then
    # because they are all after each other, just show the url, not their tags.
    goto=$(awk '{print $1}' $UZBL_BOOKMARKS_FILE | $DMENU)
else
    # show tags as well
    goto=$($DMENU < $UZBL_BOOKMARKS_FILE | awk '{print $1}')
fi

#[ -n "$goto" ] && echo "uri $goto" > $UZBL_FIFO
[ -n "$goto" ] && echo "uri $goto" | socat - unix-connect:$UZBL_SOCKET
