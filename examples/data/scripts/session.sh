#!/bin/sh
#
# Very simple session manager for uzbl-browser.
# To use, add a line like 'bind quit = spawn @scripts_dir/session.sh endsession'
# to your config.
# To restore the session, run this script with the argument "launch". An
# instance of uzbl-browser will be launched for each stored url.
#
# When called with "endsession" as the argument, it will backup
# $UZBL_SESSION_FILE, look for fifos in $UZBL_FIFO_DIR and instruct each of them
# to store its current url in $UZBL_SESSION_FILE and terminate.
#
# "endinstance" is used internally and doesn't need to be called manually.

if [ -z "$UZBL_UTIL_DIR" ]; then
    # we're being run standalone, we have to figure out where $UZBL_UTIL_DIR is
    # using the same logic as uzbl-browser does.
    UZBL_UTIL_DIR=${XDG_DATA_HOME:-$HOME/.local/share}/uzbl/scripts/util
    if ! [ -d "$UZBL_UTIL_DIR" ]; then
        PREFIX=$(grep '^PREFIX' "$(which uzbl-browser)" | sed 's/.*=//')
        UZBL_UTIL_DIR=$PREFIX/share/uzbl/examples/data/scripts/util
    fi
fi

. "$UZBL_UTIL_DIR"/uzbl-dir.sh
[ -d "$UZBL_DATA_DIR" ] || exit 1

UZBL="uzbl-browser -c $UZBL_CONFIG_FILE" # add custom flags and whatever here.

if [ $# -gt 1 ]; then
    # this script is being run from uzbl, rather than standalone
    . "$UZBL_UTIL_DIR"/uzbl-args.sh
fi

scriptfile=$0                            # this script
act="$1"

case $act in
    "launch" )
        urls=$(cat "$UZBL_SESSION_FILE")
        if [ -z "$urls" ]; then
            $UZBL
        else
            for url in $urls; do
                $UZBL --uri "$url" &
            done
        fi
        ;;

    "endinstance" )
        if [ -z "$UZBL_FIFO" ]; then
            echo "session manager: endinstance must be called from uzbl"
            exit 1
        fi
        [ "$UZBL_URL" != "(null)" ] && echo "$UZBL_URL" >> "$UZBL_SESSION_FILE"
        echo exit > "$UZBL_FIFO"
        ;;

    "endsession" )
        mv "$UZBL_SESSION_FILE" "$UZBL_SESSION_FILE~"
        for fifo in "$UZBL_FIFO_DIR"/uzbl_fifo_*; do
            if [ "$fifo" != "$UZBL_FIFO" ]; then
                echo "spawn $scriptfile endinstance" > "$fifo"
            fi
        done
        echo "spawn $scriptfile endinstance" > "$UZBL_FIFO"
        ;;

    * )
        echo "session manager: bad action"
        echo "Usage: $scriptfile [COMMAND] where commands are:"
        echo " launch      - Restore a saved session or start a new one"
        echo " endinstance - Quit the current instance. Must be called from uzbl"
        echo " endsession  - Quit the running session."
        ;;
esac
