#!/bin/sh
# Useful functions for scripts

print () {
    printf "%b" "$@"
}

error () {
    print "$@" >&2
}

sed_i () {
    local path="$1"
    readonly path

    local tmpfile="$( mktemp "$path.XXXXXX" )"
    readonly tmpfile

    sed "$@" < "$path" > "$tmpfile"
    mv "$tmpfile" "$path"
}

uzbl_send () {
    socat - "unix-connect:$UZBL_SOCKET"
}

uzbl_control () {
    print "$@" | uzbl_send
}

uzbl_escape () {
    sed -e 's/@/\\@/g'
}
