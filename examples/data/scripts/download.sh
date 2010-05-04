#!/bin/sh
# just an example of how you could handle your downloads
# try some pattern matching on the uri to determine what we should do

# Some sites block the default wget --user-agent..
GET="wget --user-agent=Firefox --content-disposition --load-cookies=$XDG_DATA_HOME/uzbl/cookies.txt"

dest="$HOME"
url="$8"

http_proxy="$9"
export http_proxy

test "x$url" = "x" && { echo "you must supply a url! ($url)"; exit 1; }

# only changes the dir for the $get sub process
if echo "$url" | grep -E '.*\.torrent' >/dev/null;
then
    ( cd "$dest"; $GET "$url")
else
    ( cd "$dest"; $GET "$url")
fi
