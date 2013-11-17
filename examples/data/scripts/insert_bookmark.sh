#!/bin/sh

. "$UZBL_UTIL_DIR/uzbl-dir.sh"
. "$UZBL_UTIL_DIR/uzbl-util.sh"

>> "$UZBL_BOOKMARKS_FILE" || exit 1

which zenity >/dev/null 2>&1 || exit 2

entry="$( zenity --text="Add bookmark. add tags after the tabulators, separated by spaces" --entry-text="$UZBL_URI	$UZBL_TITLE	" )"
exitstatus="$?"
[ "$exitstatus" -eq 0 ] || exit "$exitstatus"

# Get the fields from the entry
url="$( print "$entry" | cut -d "	" -f 1 )"
title="$( print "$entry" | cut -d "	" -f 2 )"
tags="$( print "$entry" | cut -d "	" -f 3 )"

# Escape the url and title for sed
eurl="$( print "$url" | sed -e 's!\\!\\\\!g
                                s![][^$*./]!\\&!g
                                s!#.*!!' )"

# See if the url is in the file already
cur="$( sed -n -e "\<^$eurl<pq" "$UZBL_BOOKMARKS_FILE" )"
if [ -n "$cur" ]; then
    burl="$( print "$cur" | cut -d "	" -f 1 )"
    btags="$( print "$cur" | cut -d "	" -f 3 )"

    # Remove the current entry from the bookmarks file
    sed_i "$UZBL_BOOKMARKS_FILE" -e "\<^$eurl\t<d"

    # Append the exiting tags
    tags="$tags $btags"
fi

# Remove duplicate tags (also sorts)
tags="$( print "$tags" | tr ' ' '\n' | sort -u | tr '\n' ' ' )"

# Put the tags in the file
print "$url	$title	$tags\n" >> "$UZBL_BOOKMARKS_FILE"
