#!/bin/sh

case "$( echo "script @scripts_dir/go_input.js" | socat - "unix-connect:$UZBL_SOCKET" )" in
    *XXXFORM_ACTIVEXXX*)
        echo 'event KEYCMD_CLEAR' > "$UZBL_FIFO"
        ;;
esac
