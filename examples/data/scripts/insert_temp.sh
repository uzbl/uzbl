#!/bin/sh

. "$UZBL_UTIL_DIR/uzbl-dir.sh"

>> "$UZBL_TEMPS_FILE" || exit 1

echo "$UZBL_URI $UZBL_TITLE" >> "$UZBL_TEMPS_FILE"
