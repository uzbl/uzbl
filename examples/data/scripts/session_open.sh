#!/bin/sh

#dmenu.sh is a PITA with multiple-word prompts
DMENU_PROMPT="Open_session"
. "$UZBL_UTIL_DIR/session_select.sh"

[ -n "$selection" ] && uzbl-tabbed -l "$selection"

