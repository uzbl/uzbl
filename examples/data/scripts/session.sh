#!/bin/sh
#
# Very simple session manager for uzbl-browser.
# To use, add a line like 'bind quit = spawn @scripts_dir/session.sh' to your
# config. This binding will exit every instance of uzbl and store the URLs they
# had open in $UZBL_SESSION_FILE.
#
# When a session file exists this script can be run with no arguments (or the
# argument "launch") to start an instance of uzbl-browser for every stored url.
#
# If no session file exists (or if called with "endsession" as the first
# argument), this script looks for instances of uzbl that have fifos in
# $UZBL_FIFO_DIR and instructs each of them to store its current url in
# $UZBL_SESSION_FILE and terminate.
#
# "endinstance" is used internally and doesn't need to be called manually.

if [ -z "$UZBL_UTIL_DIR" ]; then
    # we're being run standalone, we have to figure out where $UZBL_UTIL_DIR is
    # using the same logic as uzbl-browser does.
    UZBL_UTIL_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/uzbl/scripts/util"
    if ! [ -d "$UZBL_UTIL_DIR" ]; then
        PREFIX="$( grep '^PREFIX' "$( which uzbl-browser )" | sed -e 's/.*=//' )"
        UZBL_UTIL_DIR="$PREFIX/share/uzbl/examples/data/scripts/util"
    fi
fi

. "$UZBL_UTIL_DIR/uzbl-dir.sh"
. "$UZBL_UTIL_DIR/uzbl-util.sh"

[ -d "$UZBL_DATA_DIR" ] || exit 1

UZBL="uzbl-browser -c \"$UZBL_CONFIG_FILE\"" # add custom flags and whatever here.

scriptfile="$( readlink -f "$0" )"                            # this script
act="$1"

if [ -z "$act" ]; then
  [ -f "$UZBL_SESSION_FILE" ] && act="launch" || act="endsession"
fi

case $act in
    "launch")
        urls="$( cat "$UZBL_SESSION_FILE" )"
        if [ -z "$urls" ]; then
            $UZBL
        else
            for url in $urls; do
                $UZBL --uri "$url" &
            done
        mv "$UZBL_SESSION_FILE" "$UZBL_SESSION_FILE~"
        fi
        ;;

    "endinstance")
        if [ -z "$UZBL_FIFO" ]; then
            print "session manager: endinstance must be called from uzbl\n"
            exit 1
        fi
        [ "$UZBL_URI" != "(null)" ] && print "$UZBL_URI\n" >> "$UZBL_SESSION_FILE"
        uzbl_control "exit\n"
        ;;

    "endsession")
        for fifo in "$UZBL_FIFO_DIR/uzbl_fifo_*"; do
            if ! [ "$fifo" = "$UZBL_FIFO" ]; then
                print "spawn $scriptfile endinstance\n" > "$fifo"
            fi
        done
        [ -z "$UZBL_FIFO" ] || uzbl_control "spawn $scriptfile endinstance\n"
        ;;

    *)
        print "session manager: bad action\n"
        print "Usage: $scriptfile [COMMAND] where commands are:\n"
        print " launch      - Restore a saved session or start a new one\n"
        print " endinstance - Quit the current instance. Must be called from uzbl\n"
        print " endsession  - Quit the running session.\n"
        ;;
esac
