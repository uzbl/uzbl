#!/bin/sh

#NOTE: it's the job of the script that inserts bookmarks to make sure there are no dupes.

readonly DMENU_SCHEME="bookmarks"
readonly DMENU_OPTIONS="xmms vertical resize"

. "$UZBL_UTIL_DIR/dmenu.sh"
. "$UZBL_UTIL_DIR/uzbl-dir.sh"
. "$UZBL_UTIL_DIR/uzbl-util.sh"

[ -r "$UZBL_BOOKMARKS_FILE" ] || exit 1

if $DMENU_HAS_VERTICAL; then
    # show tags as well
    goto="$( $DMENU < "$UZBL_BOOKMARKS_FILE" | cut -d "	" -f 1 )"
else
    # because they are all after each other, just show the url, not their tags.
    goto="$( cut -d "	" -f 1 < "$UZBL_BOOKMARKS_FILE" | $DMENU )"
fi
readonly goto

[ -n "$goto" ] && uzbl_control "uri $goto\n"
