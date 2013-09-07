#!/bin/sh

DMENU_SCHEME="history"
DMENU_OPTIONS="xmms vertical resize"

. "$UZBL_UTIL_DIR/dmenu.sh"

#NOTE: this is coupled to a specific implementation of
#UzblTabbed.session_backup_filename, i.e. using dotfiles to hide the
#backups
selection="$( (uzbl-tabbed -L | grep -v '^\.') | $DMENU )"
