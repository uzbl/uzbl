#!/bin/sh

. "$UZBL_UTIL_DIR/uzbl-util.sh"

case "$( uzbl_control "js page file @scripts_dir/go_input.js\n" )" in
    *XXXFORM_ACTIVEXXX*)
        uzbl_control 'event KEYCMD_CLEAR\n'
        ;;
esac
