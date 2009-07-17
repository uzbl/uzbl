#!/bin/sh

# with this script you can store the current url in the clipboard, or go to the url which is stored in the clipboard.

clip=xclip

fifo="$5"
action="$1"
url="$7"

selection=`$clip -o`

case $action in
  "yank" ) echo -n "$url" | eval "$clip";;
  "goto" ) echo "uri $selection" > "$fifo";;
  * ) echo "clipboard.sh: invalid action";;
esac
