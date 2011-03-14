#!/bin/sh
# This scripts acts on the return value of followLinks in follow.js 

case "$1" in
    XXXEMIT_FORM_ACTIVEXXX)
        # a form element was selected
        printf 'event FORM_ACTIVE\nevent KEYCMD_CLEAR\n' > "$UZBL_FIFO"
        ;;
    XXXRESET_MODEXXX)
        # a link was selected, reset uzbl's input mode
        printf 'set mode=\nevent KEYCMD_CLEAR\n' > "$UZBL_FIFO"
        ;;
esac
