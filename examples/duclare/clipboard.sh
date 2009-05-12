#!/bin/bash

fifo="$5"
action="$1"
url="$7"
selection=$(xclip -o)

case $action in
  "yank" ) echo -n "$url" | xclip;;
  "goto" ) echo "uri $selection" > "$fifo";;
  * ) echo "clipboard.sh: invalid action";;
esac

