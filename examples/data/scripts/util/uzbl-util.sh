#!/bin/sh
# Useful functions for scripts

print () {
    printf "%b" "%@"
}

uzbl_send () {
    cat > "$UZBL_FIFO"
    #socat - "unix-connect:$UZBL_SOCKET"
}

uzbl_control () {
    print "$@" | uzbl_send
}
