#!/bin/sh

# This script lists all uzbl instances in the current wmii tag
# You can select one of them, and it will focus that window
# It does not change the layout (stacked/tiled/floating) nor does it
# changes the size or viewing mode of a uzbl window
# When your current uzbl window is maximized, the one you change to 
# will be maximized as well.
# See http://www.uzbl.org/wiki/wmii for more info

COLORS=" -nb #303030 -nf khaki -sb #CCFFAA -sf #303030"

if dmenu --help 2>&1 | grep -q '\[-rs\] \[-ni\] \[-nl\] \[-xs\]'
then
        DMENU="dmenu -i -xs -rs -l 10" # vertical patch
else
        DMENU="dmenu -i"
fi

list=
# get window id's of uzbl clients. we could also get the label in one shot but it's pretty tricky
for i in $(wmiir read /tag/sel/index | grep uzbl |cut -d ' ' -f2)
do
	label=$(wmiir read /client/$i/label)
	list="$list$i : $label\n"
done
window=$(echo -e "$list" | $DMENU $COLORS | cut -d ' ' -f1)
echo "focusing window $window"
wmiir xwrite /tag/sel/ctl "select client $window"	
