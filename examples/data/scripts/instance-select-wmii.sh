#!/bin/sh

# This script allows you to focus another uzbl window
# It considers all uzbl windows in the current tag
# you can select one from a list, or go to the next/previous one
# It does not change the layout (stacked/tiled/floating) nor does it
# changes the size or viewing mode of a uzbl window
# When your current uzbl window is maximized, the one you change to
# will be maximized as well.
# See http://www.uzbl.org/wiki/wmii for more info
# $1 must be one of 'list', 'next', 'prev'

readonly DMENU_SCHEME="wmii"

. "$UZBL_UTIL_DIR/dmenu.sh"
. "$UZBL_UTIL_DIR/uzbl-util.sh"

readonly action="$1"
shift

case "$action" in
"list")
    list=""
    # get window id's of uzbl clients. we could also get the label in one shot but it's pretty tricky
    for i in $( wmiir read /tag/sel/index | grep uzbl | cut -d ' ' -f 2 ); do
        label="$( wmiir read /client/$i/label )"
        list="$list$i : $label\n"
    done
    readonly list
    readonly window="$( print "$list\n" | $DMENU | cut -d ' ' -f 1 )"
    wmiir xwrite /tag/sel/ctl "select client $window"
    ;;
"next")
    readonly current="$( wmiir read /client/sel/ctl | head -n 1 )"
    # find the next uzbl window and focus it
    readonly next="$( wmiir read /tag/sel/index | grep -A 10000 " $current " | grep -m 1 uzbl | cut -d ' ' -f 2 )"
    if [ -n "$next" ]; then
        wmiir xwrite /tag/sel/ctl "select client $next"
    fi
    ;;
"prev")
    readonly current="$( wmiir read /client/sel/ctl | head -n 1 )"
    readonly prev="$( wmiir read /tag/sel/index | grep -B 10000 " $current " | tac | grep -m 1 uzbl | cut -d ' ' -f 2 )"
    if [ -n "$prev" ]; then
        wmiir xwrite /tag/sel/ctl "select client $prev"
    fi
    ;;
*)
    error "Unrecognized action: $action\n"
    exit 1
    ;;
esac
