#!/bin/sh

[ -n "${UZBL_PRIVATE-1}" ] && exit 0

. "$UZBL_UTIL_DIR/uzbl-dir.sh"

>> "$UZBL_HISTORY_FILE" || exit 1

echo "$( date +'%Y-%m-%d %H:%M:%S' ) $UZBL_URI $UZBL_TITLE" >> "$UZBL_HISTORY_FILE"
