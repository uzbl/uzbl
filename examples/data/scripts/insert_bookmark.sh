#!/bin/sh

. "$UZBL_UTIL_DIR/uzbl-dir.sh"
. "$UZBL_UTIL_DIR/uzbl-util.sh"

>> "$UZBL_BOOKMARKS_FILE" || exit 1

which zenity >/dev/null 2>&1 || exit 2

readonly entry="$( zenity --entry --text="Add bookmark. add tags after the tabulators, separated by spaces" --entry-text="$UZBL_URI	$UZBL_TITLE	" )"
readonly exitstatus="$?"
[ "$exitstatus" -eq 0 ] || exit "$exitstatus"

# Get the fields from the entry
readonly url="$( print "$entry" | cut -d "	" -f 1 )"
readonly title="$( print "$entry" | cut -d "	" -f 2 )"
readonly new_tags="$( print "$entry" | cut -d "	" -f 3 )"

# Escape the url and title for sed
readonly eurl="$( print "$url" | sed -e 's!\\!\\\\!g
                                         s![][^$*./]!\\&!g
                                         s!#.*!!' )"

# See if the url is in the file already
readonly cur="$( sed -n -e "\<^$eurl<pq" "$UZBL_BOOKMARKS_FILE" )"
if [ -n "$cur" ]; then
    readonly cur_url="$( print "$cur" | cut -d "	" -f 1 )"
    readonly cur_tags="$( print "$cur" | cut -d "	" -f 3 )"

    # Remove the current entry from the bookmarks file
    sed_i "$UZBL_BOOKMARKS_FILE" -e "\<^$eurl\t<d"

    # Append the exiting tags
    tags="$new_tags $cur_tags"
else
    tags="$new_tags"
fi
readonly tags

# Remove duplicate tags (also sorts)
readonly sorted_uniq_tags="$( print "$tags" | tr ' ' '\n' | sort -u | tr '\n' ' ' )"

# Put the tags in the file
print "$url	$title	$sorted_uniq_tags\n" >> "$UZBL_BOOKMARKS_FILE"
