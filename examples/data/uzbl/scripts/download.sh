#!/bin/sh
# just an example of how you could handle your downloads
# try some pattern matching on the uri to determine what we should do

# Some sites block the default wget --user-agent..
GET="wget --user-agent=Firefox"

dest="$HOME"
url="$8"

test "x$url" = "x" && { echo "you must supply a url! ($url)"; exit 1; }

# only changes the dir for the $get sub process
if echo "$url" | grep -E '.*\.torrent' >/dev/null;
then
    ( cd "$dest"; eval "$GET" "$url")
else
    ( cd "$dest"; eval "$GET" "$url")
fi
