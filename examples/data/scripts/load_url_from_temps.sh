#!/bin/sh

DMENU_SCHEME="temps"
DMENU_OPTIONS="xmms vertical resize"

. "$UZBL_UTIL_DIR/dmenu.sh"
. "$UZBL_UTIL_DIR/uzbl-dir.sh"
. "$UZBL_UTIL_DIR/uzbl-util.sh"

[ -r "$UZBL_TEMPS_FILE" ] || exit 1

if [ -z "$DMENU_HAS_VERTICAL" ]; then
    # because they are all after each other, just show the url, not their titles.
    goto=$( cut -d ' ' -f 1 < "$UZBL_TEMPS_FILE" | $DMENU )
else
    # show titles
    goto="$( $DMENU < "$UZBL_TEMPS_FILE" | cut -d ' ' -f 1 )"
fi

sed_i "$UZBL_TEMPS_FILE" -e "\<^$goto <d"

[ -n "$goto" ] && uzbl_control "uri $goto\n"
