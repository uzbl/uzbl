#!/bin/sh

if [ -n "$1" ]; then
    cookie_file="$1"
    shift
else
    cookie_file="${XDG_DATA_HOME:-$HOME/.local/share}/uzbl/cookies.txt"
fi

awk -F \\t '
BEGIN {
    scheme["TRUE"] = "https";
    scheme["FALSE"] = "http";
}
$0 ~ /^#HttpOnly_/ {
    gsub(/@/, "\\@")
    printf("add_cookie \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"\n", substr($1,length("#HttpOnly_"),length($1)), $3, $6, $7, scheme[$4], $5)
}
$0 !~ /^#/ {
    gsub(/@/, "\\@")
    printf("add_cookie \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"\n", $1, $3, $6, $7, scheme[$4], $5)
}
' "$cookie_file"
