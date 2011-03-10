#!/bin/sh

. "$UZBL_UTIL_DIR/uzbl-dir.sh"
. "$UZBL_UTIL_DIR/uzbl-util.sh"

>> "$UZBL_BOOKMARKS_FILE" || exit 1

which zenity >/dev/null 2>&1 || exit 2

tags="$( zenity --entry --text="Enter space-separated tags for bookmark $UZBL_URI:" )"
exitstatus="$?"
[ "$exitstatus" -eq 0 ] || exit "$exitstatus"

# Get the fields from the entry
url=$( echo -n "$entry" | cut -d "	" -f 1 )
title=$( echo -n "$entry" | cut -d "	" -f 2 )
tags=$( echo -n "$entry" | cut -d "	" -f 3 )

# See if the url is in the file already
cur=$( sed -n -e "\<^$eurl<p" "$UZBL_BOOKMARKS_FILE" | head -n 1 )
if [ -n "$cur" ]; then
    burl=$( echo -n "$cur" | cut -d "	" -f 1 )
    btitle=$( echo -n "$cur" | cut -d "	" -f 2 )
    btags=$( echo -n "$cur" | cut -d "	" -f 3 )

    # Remove the current entry from the bookmarks file
    sed -i -e "\<^$url\t<d" "$UZBL_BOOKMARKS_FILE"

    # Append the exiting tags
    tags="$tags $btags"
fi

# Remove duplicate tags (also sorts)
tags=$( echo -n "$tags" | tr ' ' '\n' | sort -u | tr '\n' ' ' )

# Put the tags in the file
print "$url	$title	$tags\n" >> "$UZBL_BOOKMARKS_FILE"
