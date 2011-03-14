#!/bin/sh

case "$( echo "script @scripts_dir/go_input.js" | socat - "unix-connect:$UZBL_SOCKET" )" in
    *XXXEMIT_FORM_ACTIVEXXX*)
        echo "event FORM_ACTIVE" > "$UZBL_FIFO"
        ;;
esac
