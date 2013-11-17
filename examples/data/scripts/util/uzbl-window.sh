#!/bin/sh
# uzbl window detection

. "$UZBL_UTIL_DIR/uzbl-util.sh"

readonly UZBL_WIN_POS="$( xwininfo -id "$UZBL_XID" | \
                          sed -n -e 's/[ ]*Corners:[ ]*[+-]\([0-9]*\)[+-]\([0-9]*\).*$/\1 \2/p' )"
readonly UZBL_WIN_SIZE="$( xwininfo -id "$UZBL_XID" | \
                           sed -n -e 's/[ ]*-geometry[ ]*\([0-9]*\)x\([0-9]*\).*$/\1 \2/p' )"
readonly UZBL_WIN_POS_X="$( print "$UZBL_WIN_POS" | cut -d ' ' -f 1 )"
readonly UZBL_WIN_POS_Y="$( print "$UZBL_WIN_POS" | cut -d ' ' -f 2 )"
readonly UZBL_WIN_WIDTH="$( print "$UZBL_WIN_SIZE" | cut -d ' ' -f 1 )"
readonly UZBL_WIN_HEIGHT="$( print "$UZBL_WIN_SIZE" | cut -d ' ' -f 2 )"
