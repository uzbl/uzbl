#!/bin/sh

source $UZBL_UTIL_DIR/uzbl-dir.sh

[ -d "$UZBL_DATA_DIR" ] || exit 1
[ -w "$UZBL_BOOKMARKS_FILE" ] || [ ! -a "$UZBL_BOOKMARKS_FILE" ] || exit 1

which zenity &>/dev/null || exit 2
# replace tabs, they are pointless in titles and we want to use tabs as delimiter.
title=$(echo "$UZBL_TITLE" | sed 's/\t/    /')
entry=$(zenity --entry --text="Add bookmark. add tags after the '\t', separated by spaces" --entry-text="$UZBL_URL $title\t")
exitstatus=$?
if [ $exitstatus -ne 0 ]; then exit $exitstatus; fi
url=$(echo $entry | awk '{print $1}')

# TODO: check if already exists, if so, and tags are different: ask if you want to replace tags
echo "$entry" >/dev/null #for some reason we need this.. don't ask me why
echo -e "$entry"  >> $UZBL_BOOKMARKS_FILE
true
