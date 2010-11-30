#!/bin/sh

# This script is just a wrapper around follow.js that lets us change uzbl's mode
# after a link is selected.

# if socat is installed then we can change Uzbl's input mode once a link is
# selected; otherwise we just select a link.
if ! which socat >/dev/null 2>&1; then
  echo 'script @scripts_dir/follow.js "@{follow_hint_keys} '$1'"' > "$UZBL_FIFO"
  exit
fi

result=$(echo 'script @scripts_dir/follow.js "@{follow_hint_keys} '$1'"' | socat - unix-connect:"$UZBL_SOCKET")
case $result in
  *XXXEMIT_FORM_ACTIVEXXX*)
    # a form element was selected
    echo 'event FORM_ACTIVE' > "$UZBL_FIFO" ;;
  *XXXRESET_MODEXXX*)
    # a link was selected, reset uzbl's input mode
    echo 'set mode=' > "$UZBL_FIFO" ;;
esac
