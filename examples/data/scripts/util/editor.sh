#!/bin/sh
# Editor selection

if [ "x$VTERM" = "x" ]; then
    VTERM="xterm"
fi

UZBL_EDITOR="$VISUAL"
if [ -z "$UZBL_EDITOR" ]; then
    if [ -z "$EDITOR" ]; then
        UZBL_EDITOR="$VTERM -e vim"
    else
        UZBL_EDITOR="$VTERM -e $EDITOR"
    fi
fi
