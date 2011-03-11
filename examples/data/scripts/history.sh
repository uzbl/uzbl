#!/bin/sh

. "$UZBL_UTIL_DIR/uzbl-dir.sh"

>> "$UZBL_HISTORY_FILE" || exit 1

print "$( date +'%Y-%m-%d %H:%M:%S' ) $UZBL_URI $UZBL_TITLE\n" >> "$UZBL_HISTORY_FILE"
