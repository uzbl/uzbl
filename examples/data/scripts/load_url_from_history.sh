#!/bin/sh

readonly DMENU_SCHEME="history"
readonly DMENU_OPTIONS="xmms vertical resize"

. "$UZBL_UTIL_DIR/dmenu.sh"
. "$UZBL_UTIL_DIR/uzbl-dir.sh"
. "$UZBL_UTIL_DIR/uzbl-util.sh"

[ -r "$UZBL_HISTORY_FILE" ] || exit 1

# choose from all entries, sorted and uniqued
if $DMENU_HAS_VERTICAL; then
    # choose an item in reverse order, showing also the date and page titles

    # tac: output history in reverse, awk: remove duplicate URLs, $DMENU: present to dmenu,
    # cut: get the first three fields, awk: pick the last field
    # As opposed to just getting the third field directly, the combination of cut+awk allows
    # to enter a completely new URL (which would be field number 1 or $NF, but not 3).
    goto="$( tac "$UZBL_HISTORY_FILE" | awk '!a[$3]++' | $DMENU | cut -d ' ' -f -3  | awk '{ print $NF }' )"
else
    readonly current="$( tail -n 1 "$UZBL_HISTORY_FILE" | cut -d ' ' -f 3 )"
    goto="$( ( print "$current\n"; cut -d ' ' -f 3 < "$UZBL_HISTORY_FILE" | grep -v -e "^$current\$" | sort -u ) | $DMENU )"
fi
readonly goto

[ -n "$goto" ] && uzbl_control "uri $goto\n"
