#!/bin/sh
# dmenu setup

case "$DMENU_SCHEME" in
    # wmii
    "wmii" )
        NB="#303030"
        NF="khaki"
        SB="#ccffaa"
        SF="#303030"
        ;;
    # Formfiller
    "formfiller" )
        NB="#0f0f0f"
        NF="#4e7093"
        SB="#003d7c"
        SF="#3a9bff"
        ;;
    # Bookmarks
    "bookmarks" )
        NB="#303030"
        NF="khaki"
        SB="#ccffaa"
        SF="#303030"
        ;;
    # History
    "history" )
        NB="#303030"
        NF="khaki"
        SB="#ccffaa"
        SF="#303030"
        ;;
    # Temps
    "temps" )
        NB="#303030"
        NF="khaki"
        SB="#ccffaa"
        SF="#303030"
        ;;
    # Default
    * )
        NB="#303030"
        NF="khaki"
        SB="#ccffaa"
        SF="#303030"
        ;;
esac

DMENU_COLORS="-nb $NB -nf $NF -sb $SB -sf $SF"

# Default arguments
if [ -z "$DMENU_ARGS" ]; then
    DMENU_ARGS="-i"
fi

# Set the font if wanted
if [ -n "$DMENU_FONT" ]; then
    DMENU_ARGS="$DMENU_ARGS -fn $DMENU_FONT"
fi

# Set the prompt if wanted
if [ -n "$DMENU_PROMPT" ]; then
    DMENU_ARGS="$DMENU_ARGS -p $DMENU_PROMPT"
fi

# Detect the xmms patch
if dmenu --help 2>&1 | grep -q '\[-xs\]'; then
    DMENU_XMMS_ARGS="-xs"
    DMENU_HAS_XMMS=1
fi

# Detect the tok patch
if dmenu --help 2>&1 | grep -q '\[-t\]'; then
    DMENU_XMMS_ARGS="-t"
    DMENU_HAS_XMMS=1
fi

if echo $DMENU_OPTIONS | grep -q -w 'xmms'; then
    DMENU_ARGS="$DMENU_ARGS $DMENU_XMMS_ARGS"
fi

# Detect the vertical patch
if dmenu --help 2>&1 | grep -q '\[-l <\?lines>\?\]'; then
    # Default to 10 lines
    if [ -z "$DMENU_LINES" ]; then
        DMENU_LINES=10
    fi

    DMENU_VERTICAL_ARGS="-l $DMENU_LINES"
    DMENU_HAS_VERTICAL=1

    # Detect the resize patch
    if dmenu --help 2>&1 | grep -q '\[-rs\]'; then
        DMENU_RESIZE_ARGS="-rs"
        DMENU_HAS_RESIZE=1
    fi

    if echo $DMENU_OPTIONS | grep -q -w 'vertical'; then
        DMENU_ARGS="$DMENU_ARGS $DMENU_VERTICAL_ARGS"

        if echo $DMENU_OPTIONS | grep -q -w 'resize'; then
            DMENU_ARGS="$DMENU_ARGS $DMENU_RESIZE_ARGS"
        fi
    fi
fi

# Detect placement patch
if dmenu --help 2>&1 | grep -q '\[-x <\?xoffset>\?\]'; then
    DMENU_PLACE_X="-x"
    DMENU_PLACE_Y="-y"
    DMENU_PLACE_WIDTH="-w"
    DMENU_PLACE_HEIGHT="-h"
    DMENU_HAS_PLACEMENT=1
fi

DMENU="dmenu $DMENU_ARGS $DMENU_COLORS"
