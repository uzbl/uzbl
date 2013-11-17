#!/bin/sh
# Common directories and files used in scripts

# Common things first
readonly UZBL_DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/uzbl"
readonly UZBL_CONFIG_DIR="${XDG_CONFIG_DIR:-$HOME/.config}/uzbl"
if [ -z "$XDG_RUNTIME_DIR" ]; then
    UZBL_FIFO_DIR="/tmp/uzbl-$USER"
    UZBL_SOCKET_DIR="/tmp/uzbl-$USER"
else
    UZBL_FIFO_DIR="$XDG_RUNTIME_DIR/uzbl"
    UZBL_SOCKET_DIR="$XDG_RUNTIME_DIR/uzbl"
fi
mkdir -p "$UZBL_FIFO_DIR"
mkdir -p "$UZBL_SOCKET_DIR"
readonly UZBL_FIFO_DIR
readonly UZBL_SOCKET_DIR

# Directories
readonly UZBL_DOWNLOAD_DIR="${XDG_DOWNLOAD_DIR:-$HOME}"
readonly UZBL_FORMS_DIR="${UZBL_FORMS_DIR:-$UZBL_DATA_DIR/dforms}"

# Data files
readonly UZBL_CONFIG_FILE="${UZBL_CONFIG_FILE:-$UZBL_CONFIG_DIR/config}"
readonly UZBL_COOKIE_FILE="${UZBL_COOKIE_FILE:-$UZBL_DATA_DIR/cookies.txt}"
readonly UZBL_SESSION_COOKIE_FILE="${UZBL_SESSION_COOKIE_FILE:-$UZBL_DATA_DIR/cookies-session.txt}"
readonly UZBL_BOOKMARKS_FILE="${UZBL_BOOKMARKS_FILE:-$UZBL_DATA_DIR/bookmarks}"
readonly UZBL_TEMPS_FILE="${UZBL_TEMPS_FILE:-$UZBL_DATA_DIR/temps}"
readonly UZBL_HISTORY_FILE="${UZBL_HISTORY_FILE:-$UZBL_DATA_DIR/history}"
readonly UZBL_SESSION_FILE="${UZBL_SESSION_FILE:-$UZBL_DATA_DIR/browser-session}"
