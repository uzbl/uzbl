#!/bin/bash

# with this script you can store the current url in the clipboard, or go to the url which is stored in the clipboard.

fifo="$5"
action="$1"
url="$7"
selection=$(xclip -o)

case $action in
  "yank" ) echo -n "$url" | xclip;;
  "goto" ) echo "act uri $selection" > "$fifo";;
  * ) echo "clipboard.sh: invalid action";;
esac

