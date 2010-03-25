#!/bin/sh


# This script allows you to focus another uzbl window
# It considers all uzbl windows in the current tag
# you can select one from a list, or go to the next/previous one
# It does not change the layout (stacked/tiled/floating) nor does it
# changes the size or viewing mode of a uzbl window
# When your current uzbl window is maximized, the one you change to
# will be maximized as well.
# See http://www.uzbl.org/wiki/wmii for more info
# $1 must be one of 'list', 'next', 'prev'

COLORS=" -nb #303030 -nf khaki -sb #CCFFAA -sf #303030"

if dmenu --help 2>&1 | grep -q '\[-rs\] \[-ni\] \[-nl\] \[-xs\]'
then
        DMENU="dmenu -i -xs -rs -l 10" # vertical patch
else
        DMENU="dmenu -i"
fi

if [ "$1" == 'list' ]
then
	list=
	# get window id's of uzbl clients. we could also get the label in one shot but it's pretty tricky
	for i in $(wmiir read /tag/sel/index | grep uzbl |cut -d ' ' -f2)
	do
		label=$(wmiir read /client/$i/label)
		list="$list$i : $label\n"
	done
	window=$(echo -e "$list" | $DMENU $COLORS | cut -d ' ' -f1)
	wmiir xwrite /tag/sel/ctl "select client $window"
elif [ "$1" == 'next' ]
then
	current=$(wmiir read /client/sel/ctl | head -n 1)
	# find the next uzbl window and focus it
	next=$(wmiir read /tag/sel/index | grep -A 10000 " $current " | grep -m 1 uzbl | cut -d ' ' -f2)
	if [ x"$next" != "x" ]
	then
		wmiir xwrite /tag/sel/ctl "select client $next"
	fi
elif [ "$1" == 'prev' ]
then
	current=$(wmiir read /client/sel/ctl | head -n 1)
	prev=$(wmiir read /tag/sel/index | grep -B 10000 " $current " | tac | grep -m 1 uzbl | cut -d ' ' -f2)
	if [ x"$prev" != "x" ]
	then
		wmiir xwrite /tag/sel/ctl "select client $prev"
	fi
else
	echo "\$1 not valid" >&2
	exit 2
fi
