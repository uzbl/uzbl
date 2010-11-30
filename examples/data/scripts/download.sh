#!/bin/sh
# just an example of how you could handle your downloads
# try some pattern matching on the uri to determine what we should do

. $UZBL_UTIL_DIR/uzbl-args.sh
. $UZBL_UTIL_DIR/uzbl-dir.sh

# Some sites block the default wget --user-agent..
GET="wget --user-agent=Firefox --content-disposition --load-cookies=$UZBL_COOKIE_JAR"

url="$1"

http_proxy="$2"
export http_proxy

if [ -z "$url" ]; then
    echo "you must supply a url! ($url)"
    exit 1
fi

# only changes the dir for the $get sub process
if echo "$url" | grep -E '.*\.torrent' >/dev/null; then
    ( cd "$UZBL_DOWNLOAD_DIR"; $GET "$url")
else
    ( cd "$UZBL_DOWNLOAD_DIR"; $GET "$url")
fi
