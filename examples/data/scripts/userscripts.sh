#!/bin/sh

scripts_dir="$XDG_DATA_HOME/uzbl/userscripts"

for SCRIPT in $(grep -rlx "\s*//\s*==UserScript==\s*" "$scripts_dir")
do
	$XDG_DATA_HOME/uzbl/scripts/userscript.sh "$4" "$6" "$SCRIPT"
done
