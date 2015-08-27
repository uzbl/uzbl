#!/bin/sh
# dmenu setup

case "$DMENU_SCHEME" in
# wmii
"wmii")
    NB="#303030"
    NF="khaki"
    SB="#ccffaa"
    SF="#303030"
    ;;
# Formfiller
"formfiller")
    NB="#0f0f0f"
    NF="#4e7093"
    SB="#003d7c"
    SF="#3a9bff"
    ;;
# Bookmarks
"bookmarks")
    NB="#303030"
    NF="khaki"
    SB="#ccffaa"
    SF="#303030"
    ;;
# History
"history")
    NB="#303030"
    NF="khaki"
    SB="#ccffaa"
    SF="#303030"
    ;;
# Temps
"temps")
    NB="#303030"
    NF="khaki"
    SB="#ccffaa"
    SF="#303030"
    ;;
# Default
*)
    NB="#303030"
    NF="khaki"
    SB="#ccffaa"
    SF="#303030"
    ;;
esac

dmenu_check_flag () {
    local flag="$1"
    readonly flag
    shift

    dmenu --help 2>&1 | grep -q '\['"$flag"'\]'
}

dmenu_check_feature () {
    local feature="$1"
    readonly feature
    shift

    echo "$DMENU_OPTIONS" | grep -q -w -e "$feature"
}

readonly DMENU_COLORS="-nb $NB -nf $NF -sb $SB -sf $SF"

# Default arguments
[ -z "$DMENU_ARGS" ] && DMENU_ARGS="-i"

# Set the font if wanted
[ -n "$DMENU_FONT" ] && DMENU_ARGS="$DMENU_ARGS -fn $DMENU_FONT"

# Set the prompt if wanted
[ -n "$DMENU_PROMPT" ] && DMENU_ARGS="$DMENU_ARGS -p $DMENU_PROMPT"

DMENU_HAS_XMMS=false
if dmenu_check_feature "xmms"; then
    # Detect the xmms patch
    if dmenu_check_flag "-xs"; then
        DMENU_XMMS_ARGS="-xs"
        DMENU_HAS_XMMS=true
    # Detect the tok patch
    elif dmenu_check_flag "-t"; then
        DMENU_XMMS_ARGS="-t"
        DMENU_HAS_XMMS=true
    fi
    readonly DMENU_XMMS_ARGS
fi
readonly DMENU_HAS_XMMS

$DMENU_HAS_XMMS && DMENU_ARGS="$DMENU_ARGS $DMENU_XMMS_ARGS"

# Detect the vertical patch
DMENU_HAS_VERTICAL=false
DMENU_HAS_RESIZE=false
if dmenu_check_feature "vertical" && dmenu_check_flag '-l <\?lines>\?'; then
    # Default to 10 lines
    if [ -z "$DMENU_LINES" ]; then
        readonly DMENU_LINES=10
    fi

    readonly DMENU_VERTICAL_ARGS="-l $DMENU_LINES"
    DMENU_HAS_VERTICAL=true

    if dmenu_check_feature "resize" && dmenu_check_flag "-rs"; then
        readonly DMENU_RESIZE_ARGS="-rs"
        DMENU_HAS_RESIZE=true
    fi
fi
readonly DMENU_HAS_VERTICAL
readonly DMENU_HAS_RESIZE

$DMENU_HAS_VERTICAL && DMENU_ARGS="$DMENU_ARGS $DMENU_VERTICAL_ARGS"
$DMENU_HAS_RESIZE && DMENU_ARGS="$DMENU_ARGS $DMENU_RESIZE_ARGS"

# Detect placement patch
DMENU_HAS_PLACEMENT=false
if dmenu_check_feature "placement" && dmenu_check_flag '-x <\?x\?offset>\?'; then
    readonly DMENU_PLACE_X="-x"
    readonly DMENU_PLACE_Y="-y"
    readonly DMENU_PLACE_WIDTH="-w"
    readonly DMENU_PLACE_HEIGHT="-h"
    DMENU_HAS_PLACEMENT=true
fi
readonly DMENU_HAS_PLACEMENT

dmenu_make_placement () {
    if ! $DMENU_HAS_PLACEMENT; then
        readonly DMENU_PLACEMENT_ARGS=""
        return
    fi

    local x="$1"
    readonly x
    shift

    local y="$1"
    readonly y
    shift

    local w="$1"
    readonly w
    shift

    local h="$1"
    readonly h
    shift

    DMENU_PLACEMENT_ARGS=""
    [ -n "$x" ] && DMENU_PLACEMENT_ARGS="$DMENU_PLACEMENT_ARGS $DMENU_PLACE_X $x"
    [ -n "$y" ] && DMENU_PLACEMENT_ARGS="$DMENU_PLACEMENT_ARGS $DMENU_PLACE_Y $y"
    [ -n "$w" ] && DMENU_PLACEMENT_ARGS="$DMENU_PLACEMENT_ARGS $DMENU_PLACE_WIDTH $w"
    [ -n "$h" ] && DMENU_PLACEMENT_ARGS="$DMENU_PLACEMENT_ARGS $DMENU_PLACE_HEIGHT $h"
    readonly DMENU_PLACEMENT_ARGS
}

readonly DMENU_ARGS
readonly DMENU="dmenu $DMENU_ARGS $DMENU_COLORS"
