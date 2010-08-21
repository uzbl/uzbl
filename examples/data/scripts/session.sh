#!/bin/sh

# Very simple session manager for uzbl-browser.  When called with "endsession" as the
# argument, it'll backup $sessionfile, look for fifos in $fifodir and
# instruct each of them to store their current url in $sessionfile and
# terminate themselves.  Run with "launch" as the argument and an instance of
# uzbl-browser will be launched for each stored url.  "endinstance" is used internally
# and doesn't need to be called manually at any point.
# Add a line like 'bind quit = /path/to/session.sh endsession' to your config

source $UZBL_UTIL_DIR/uzbl-dir.sh

[ -d $UZBL_DATA_DIR ] || exit 1

scriptfile=$0                            # this script
UZBL="uzbl-browser -c $UZBL_CONFIG_FILE" # add custom flags and whatever here.

thisfifo="$4"
act="$8"
url="$6"

if [ "$act." = "." ]; then
   act="$1"
fi


case $act in
  "launch" )
    urls=$(cat $UZBL_SESSION_FILE)
    if [ "$urls." = "." ]; then
      $UZBL
    else
      for url in $urls; do
        $UZBL --uri "$url" &
      done
    fi
    exit 0
    ;;

  "endinstance" )
    if [ "$url" != "(null)" ]; then
      echo "$url" >> $UZBL_SESSION_FILE
    fi
    echo "exit" > "$thisfifo"
    ;;

  "endsession" )
    mv "$UZBL_SESSION_FILE" "$UZBL_SESSION_FILE~"
    for fifo in $UZBL_FIFO_DIR/uzbl_fifo_*; do
      if [ "$fifo" != "$thisfifo" ]; then
        echo "spawn $scriptfile endinstance" > "$fifo"
      fi
    done
    echo "spawn $scriptfile endinstance" > "$thisfifo"
    ;;

  * ) echo "session manager: bad action"
      echo "Usage: $scriptfile [COMMAND] where commands are:"
      echo " launch 	- Restore a saved session or start a new one"
      echo " endsession	- Quit the running session. Must be called from uzbl"
      ;;
esac
