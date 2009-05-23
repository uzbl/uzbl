#!/bin/bash

# simple login form filler for uzbl.
# put the form entry values you want to add (eg login information) in the file $keydir/<domain>
# in the format <fieldname>: <value>
# (these files can be automatically created for you by setting editor and triggering this script on a site without a config)

[ -d /usr/share/uzbl/examples/data/forms  ] && keydir=/usr/share/uzbl/examples/data/forms  # you will probably get permission denied errors here.
[ -d $XDG_DATA_HOME/uzbl/forms            ] && keydir=$XDG_DATA_HOME/uzbl/forms
[ -d ./examples/data/forms                ] && keydir=./examples/data/forms #useful when developing
[ -z "$keydir" ] && exit 1

#editor=gvim
editor='urxvt -e vim'

config=$1; shift
pid=$1;    shift
xid=$1;    shift
fifo=$1;   shift
socket=$1; shift
url=$1;    shift
title=$1;  shift

[ -d $keydir ] || mkdir $keydir || exit 1

domain=$(echo $url | sed -re 's|(http\|https)+://([A-Za-z0-9\.]+)/.*|\2|')
if [[ -e $keydir/$domain ]]; then
	gawk -F': ' '{ print "act js document.getElementsByName(\"" $1 "\")[0].value = \"" $2 "\";"}' $keydir/$domain >> $fifo
else
	curl "$url" | grep '<input' | sed -nre 's|.*<input.*?name="([[:graph:]]+)".*?/>.*|\1: |p' > $keydir/$domain
	$editor $keydir/$domain
fi
