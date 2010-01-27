#!/bin/bash

# simple html form (eg for logins) filler (and manager) for uzbl.
# uses settings files like: $keydir/<domain>
# files contain lines like: <fieldname>: <value>


# user arg 1:
# edit: force editing the file (falls back to new if not found)
# new:  start with a new file.
# load: try to load from file into form

# something else (or empty): if file not available: new, otherwise load.

keydir=${XDG_DATA_HOME:-$HOME/.local/share}/uzbl/forms
[ -d "`dirname $keydir`" ] || exit 1
[ -d "$keydir" ] || mkdir "$keydir"

editor=${VISUAL}
if [[ -z ${editor} ]]; then
    #editor='gvim'
    editor='urxvt -e vim'
fi

config=$1; shift
pid=$1;    shift
xid=$1;    shift
fifo=$1;   shift
socket=$1; shift
url=$1;    shift
title=$1;  shift
action=$1

[ -d $keydir ] || mkdir $keydir || exit 1

if [ "$action" != 'edit' -a  "$action" != 'new' -a "$action" != 'load' ]
then
	action=new
	[[ -e $keydir/$domain ]] && action=load
elif [ "$action" == 'edit' ] && [[ ! -e $keydir/$domain ]]
then
	action=new
fi
domain=$(echo $url | sed -re 's|(http\|https)+://([A-Za-z0-9\.]+)/.*|\2|')


#regex='s|.*<input.*?name="([[:graph:]]+)".*?/>.*|\1: |p' # sscj's first version, does not work on http://wiki.archlinux.org/index.php?title=Special:UserLogin&returnto=Main_Page
 regex='s|.*<input.*?name="([^"]*)".*|\1: |p' #works on arch wiki, but not on http://lists.uzbl.org/listinfo.cgi/uzbl-dev-uzbl.org TODO: improve


if [ "$action" = 'load' ]
then
	[[ -e $keydir/$domain ]] || exit 2
	gawk -F': ' '{ print "js document.getElementsByName(\"" $1 "\")[0].value = \"" $2 "\";"}' $keydir/$domain >> $fifo
else
	if [ "$action" == 'new' ]
	then
		curl "$url" | grep '<input' | sed -nre "$regex" > $keydir/$domain
	fi
	[[ -e $keydir/$domain ]] || exit 3 #this should never happen, but you never know.
	$editor $keydir/$domain #TODO: if user aborts save in editor, the file is already overwritten
fi
