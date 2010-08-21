#!/bin/sh

source $UZBL_UTIL_DIR/uzbl-dir.sh

[ -r "$UZBL_HISTORY_FILE" ] || exit 1

# choose from all entries, sorted and uniqued
# goto=$(awk '{print $3}' $UZBL_HISTORY_FILE | sort -u | dmenu -i)
COLORS=" -nb #303030 -nf khaki -sb #CCFFAA -sf #303030"
if dmenu --help 2>&1 | grep -q '\[-rs\] \[-ni\] \[-nl\] \[-xs\]'; then
        DMENU="dmenu -i -xs -rs -l 10" # vertical patch
        # choose an item in reverse order, showing also the date and page titles
        # pick the last field from the first 3 fields. this way you can pick a url (prefixed with date & time) or type just a new url.
        goto=$(tac $UZBL_HISTORY_FILE | $DMENU $COLORS | cut -d ' ' -f -3  | awk '{print $NF}')
else
        DMENU="dmenu -i"
	# choose from all entries (no date or title), the first one being current url, and after that all others, sorted and uniqued, in ascending order
	current=$(tail -n 1 $UZBL_HISTORY_FILE | awk '{print $3}');
  goto=$((echo $current; awk '{print $3}' $UZBL_HISTORY_FILE | grep -v "^$current\$" \
      | sort -u) | $DMENU $COLORS)
fi

[ -n "$goto" ] && echo "uri $goto" > $4
#[ -n "$goto" ] && echo "uri $goto" | socat - unix-connect:$5
