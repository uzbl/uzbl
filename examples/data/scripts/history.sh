#!/bin/sh

. "$UZBL_UTIL_DIR/uzbl-dir.sh"

[ -w "$UZBL_HISTORY_FILE" ] || [ ! -a "$UZBL_HISTORY_FILE" ] || exit 1

printf "$( date +'%Y-%m-%d %H:%M:%S' ) $UZBL_URI $UZBL_TITLE\n" >> "$UZBL_HISTORY_FILE"
