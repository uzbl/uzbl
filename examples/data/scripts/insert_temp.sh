#!/bin/sh

. "$UZBL_UTIL_DIR/uzbl-dir.sh"
. "$UZBL_UTIL_DIR/uzbl-util.sh"

>> "$UZBL_TEMPS_FILE" || exit 1

print "$UZBL_URI $UZBL_TITLE\n" >> "$UZBL_TEMPS_FILE"
