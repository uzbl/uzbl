#!/bin/sh

. "$UZBL_UTIL_DIR/uzbl-util.sh"

case "$( print "script @scripts_dir/go_input.js\n" | socat - "unix-connect:$UZBL_SOCKET" )" in
    *XXXEMIT_FORM_ACTIVEXXX*)
        print "event FORM_ACTIVE\n" > "$UZBL_FIFO"
        ;;
esac
