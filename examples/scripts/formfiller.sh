#!/bin/bash

# simple login form filler for uzbl.
# put your login information in the file $keydir/<domain>
# in the format <fieldname>: <value>

keydir=$XDG_CONFIG_HOME/uzbl/keys
editor=gvim

config=$1; shift
pid=$1;		 shift
xid=$1;		 shift
fifo=$1;	 shift
socket=$1; shift
url=$1;		 shift
title=$1;	 shift

domain=$(echo $url | sed -re 's|(http\|https)+://([A-Za-z0-9\.]+)/.*|\2|')
if [[ -e $keydir/$domain ]]; then
	gawk -F': ' '{ print "act script document.getElementsByName(\"" $1 "\")[0].value = \"" $2 "\";"}' $keydir/$domain >> $fifo
else
	curl "$url" | grep '<input' | sed -nre 's|.*<input.*?name="([[:graph:]]+)".*?/>.*|\1: |p' > $keydir/$domain
	$editor $keydir/$domain
fi
