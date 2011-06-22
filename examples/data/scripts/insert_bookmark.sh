#!/bin/sh

. "$UZBL_UTIL_DIR/uzbl-dir.sh"

>> "$UZBL_BOOKMARKS_FILE" || exit 1

which zenity >/dev/null 2>&1 || exit 2

# Check if URL exists in Bookmarks
if grep "^$UZBL_URI" "$UZBL_BOOKMARKS_FILE"; then

    # Escape all special characters in sed and then filter tags
    oldtags="$( echo "$UZBL_URI" | sed -e 's/\(\/\|\\\|&\)/\\&/g' -n -e 's/^'$uri_sanitized' //p' )"

    zenity --title "Replace Bookmark" --question --text="A bookmark for this website already exists with tags: $oldtags. \
\nWould you like to replace existing bookmark with this one\?"

    [ "$?" -eq 0 ] || exit 1

    grep -v "^$UZBL_URI" "$UZBL_BOOKMARKS_FILE" > bookmarks.tmp
    mv bookmarks.tmp "$UZBL_BOOKMARKS_FILE" 
    rm bookmarks.tmp
fi

tags="$( zenity --title "Add Bookmark" --entry --text="Enter space-separated tags for bookmark $UZBL_URI:" )"
[ "$?" -eq 0 ] || exit 1

echo "$UZBL_URI $tags" >> "$UZBL_BOOKMARKS_FILE"
