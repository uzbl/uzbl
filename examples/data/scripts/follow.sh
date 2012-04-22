#!/bin/sh
# This scripts acts on the return value of followLinks in follow.js

result=$1
shift

case "$result" in
    XXXFORM_ACTIVEXXX)
        # a form element was selected
        echo 'event KEYCMD_CLEAR' > "$UZBL_FIFO"
        ;;
    XXXRESET_MODEXXX)
        # a link was selected, reset uzbl's input mode
        printf 'set mode=\nevent KEYCMD_CLEAR\n' > "$UZBL_FIFO"
        ;;
    XXXNEW_WINDOWXXX*)
        echo "set mode=
event KEYCMD_CLEAR
event NEW_WINDOW $@" > "$UZBL_FIFO"
        ;;
    XXXRETURNED_URIXXX*)
        uriaction=$1
        uri=${result#XXXRETURNED_URIXXX}

        case "$uriaction" in
            set)
                printf 'uri '"$uri"'\n' | sed -e 's/@/\\@/' > "$UZBL_FIFO"
                ;;
            clipboard)
                printf "$uri" | xclip
                ;;
        esac
        printf 'set mode=\nevent KEYCMD_CLEAR\n' > "$UZBL_FIFO"
esac
