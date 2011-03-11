#!/bin/sh

case "$( printf "script @scripts_dir/go_input.js\n" | socat - "unix-connect:$UZBL_SOCKET" )" in
    *XXXEMIT_FORM_ACTIVEXXX*)
        printf "event FORM_ACTIVE\n" > "$UZBL_FIFO"
        ;;
esac
