#!/bin/sh

# Very simple session manager for uzbl-browser.  When called with "endsession" as the
# argument, it'll backup $sessionfile, look for fifos in $fifodir and
# instruct each of them to store their current url in $sessionfile and
# terminate themselves.  Run with "launch" as the argument and an instance of
# uzbl-browser will be launched for each stored url.  "endinstance" is used internally
# and doesn't need to be called manually at any point.
# Add a line like 'bind quit = /path/to/session.sh endsession' to your config

source $UZBL_UTIL_DIR/uzbl-args.sh
source $UZBL_UTIL_DIR/uzbl-dir.sh

[ -d $UZBL_DATA_DIR ] || exit 1

scriptfile=$0                            # this script
UZBL="uzbl-browser -c $UZBL_CONFIG_FILE" # add custom flags and whatever here.

act="$1"

# Test if we were run alone or from uzbl
if [ -z "$UZBL_SOCKET" ]; then
    # Take the old config
    act="$UZBL_CONFIG"
fi

case $act in
    "launch" )
        urls=$(cat $UZBL_SESSION_FILE)
        if [ -z "$urls" ]; then
            $UZBL
        else
            for url in $urls; do
                $UZBL --uri "$url" &
                disown
            done
        fi
        exit 0
        ;;

    "endinstance" )
        if [ -z "$UZBL_SOCKET" ]; then
            echo "session manager: endinstance must be called from uzbl"
            exit 1
        fi
        if [ ! "$UZBL_URL" = "(null)" ]; then
            echo "$UZBL_URL" >> $UZBL_SESSION_FILE
        fi
        echo "exit" | socat - unix-connect:$UZBL_SOCKET
        ;;

    "endsession" )
        mv "$UZBL_SESSION_FILE" "$UZBL_SESSION_FILE~"
        for sock in $UZBL_SOCKET_DIR/uzbl_fifo_*; do
            if [ "$sock" != "$UZBL_SOCKET" ]; then
                echo "spawn $scriptfile endinstance" | socat - unix-connect:$socket
            fi
        done
        echo "spawn $scriptfile endinstance" | socat - unix-connect:$UZBL_SOCKET
        ;;

    * )
        echo "session manager: bad action"
        echo "Usage: $scriptfile [COMMAND] where commands are:"
        echo " launch      - Restore a saved session or start a new one"
        echo " endinstance - Quit the current instance. Must be called from uzbl"
        echo " endsession  - Quit the running session."
        ;;
esac
