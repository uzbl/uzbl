#!/bin/sh

. "$UZBL_UTIL_DIR/uzbl-dir.sh"

>> "$UZBL_BOOKMARKS_FILE" || exit 1

which zenity >/dev/null 2>&1 || exit 2

# Check if URL exists in Bookmarks
if grep "^$UZBL_URI" "$UZBL_BOOKMARKS_FILE" 2>&1 > /dev/null ; then

    # Escape special characters (used in sed) and filter tags
    uri_sanitized="$( echo "$UZBL_URI" | sed 's/\(\.\|\/\|\*\|\[\|\]\|\\\)/\\&/g' )"
    oldtags="$( sed -n 's/^'$uri_sanitized' //p' < "$UZBL_BOOKMARKS_FILE" )"

    zenity --question --title="Replace Bookmark" --text="A bookmark for <b>$UZBL_URI</b> already exists with tags: \
<b>$oldtags</b>.\n\nWould you like to replace the existing bookmark with a new one\?"

    [ "$?" -eq 0 ] || exit 0

    # Delete old bookmark
    newfile="$( grep -v "^$UZBL_URI" "$UZBL_BOOKMARKS_FILE" )"
    echo "$newfile" > "$UZBL_BOOKMARKS_FILE" 
fi

tags="$( zenity --entry --text="Enter space-separated tags for bookmark $UZBL_URI:" )"

echo "$UZBL_URI $tags" >> "$UZBL_BOOKMARKS_FILE"
