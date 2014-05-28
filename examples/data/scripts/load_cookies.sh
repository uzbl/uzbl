#!/bin/sh

. "$UZBL_UTIL_DIR/uzbl-dir.sh"

readonly path="$1"

if [ -n "$path" ]; then
    cookie_file="$path"
    shift
else
    cookie_file="$UZBL_COOKIE_FILE"
fi
readonly cookie_file

awk -F \\t '
BEGIN {
    scheme["TRUE"] = "https";
    scheme["FALSE"] = "http";
}
$0 ~ /^#HttpOnly_/ {
    gsub(/@/, "\\@")
    printf("cookie add \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"\n", substr($1,length("#HttpOnly_")+1,length($1)), $3, $6, $7, scheme[$4], $5)
}
$0 !~ /^#/ {
    gsub(/@/, "\\@")
    printf("cookie add \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"\n", $1, $3, $6, $7, scheme[$4], $5)
}
' "$cookie_file"
