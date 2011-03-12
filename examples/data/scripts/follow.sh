#!/bin/sh

# This script is just a wrapper around follow.js that lets us change uzbl's mode
# after a link is selected.

. "$UZBL_UTIL_DIR/uzbl-util.sh"

key_variable="$1"
shift

keys="$1"
shift

# if socat is installed then we can change Uzbl's input mode once a link is
# selected; otherwise we just select a link.
if ! which socat >/dev/null 2>&1; then
    print "script @scripts_dir/follow.js \"@{$key_variable} $keys\"\n" > "$UZBL_FIFO"
    exit 0
fi

result="$( print "script @scripts_dir/follow.js \"@{$key_variable} $keys\"\n" | socat - "unix-connect:$UZBL_SOCKET" )"
case $result in
    *XXXEMIT_FORM_ACTIVEXXX*)
        # a form element was selected
        print "event FORM_ACTIVE\n" > "$UZBL_FIFO"
        ;;
    *XXXRESET_MODEXXX*)
        # a link was selected, reset uzbl's input mode
        print "set mode=\n" > "$UZBL_FIFO"
        ;;
esac
