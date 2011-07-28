#!/bin/sh
# This scripts acts on the return value of followLinks in follow.js 

result=$1
shift

uriaction=$1
shift

case "$result" in
    XXXEMIT_FORM_ACTIVEXXX)
        # a form element was selected
        printf 'event KEYCMD_CLEAR\n' > "$UZBL_FIFO"
        ;;
    XXXRESET_MODEXXX)
        # a link was selected, reset uzbl's input mode
        printf 'set mode=\nevent KEYCMD_CLEAR\n' > "$UZBL_FIFO"
        ;;
    XXXRETURNED_URIXXX*)
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
