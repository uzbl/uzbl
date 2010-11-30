#!/bin/sh
# uzbl window detection

if [ -z "$UZBL_XID" ]; then
    echo "Error: UZBL_XID not set"
    echo "Please source uzbl-args.sh first"
    exit 1
fi

UZBL_WIN_POS=$(xwininfo -id $UZBL_XID | \
               sed -ne 's/Corners:[ ]*[+-]\([0-9]*\)[+-]\([0-9]*\).*$/\1 \2/p')
UZBL_WIN_SIZE=$(xwininfo -id $UZBL_XID | \
                sed -ne 's/-geometry[ ]*\([0-9]*\)x\([0-9]*\).*$/\1 \2/p')
UZBL_WIN_POS_X=$(echo $UZBL_WIN_POS | cut -d\  -f1)
UZBL_WIN_POS_Y=$(echo $UZBL_WIN_POS | cut -d\  -f2)
UZBL_WIN_WIDTH=$(echo $UZBL_WIN_SIZE | cut -d\  -f1)
UZBL_WIN_HEIGHT=$(echo $UZBL_WIN_SIZE | cut -d\  -f2)
