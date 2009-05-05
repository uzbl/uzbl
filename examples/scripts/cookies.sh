#!/bin/bash
# this is an example script of how you could manage your cookies..
# you probably want your cookies config file in your $XDG_CONFIG_HOME ( eg $HOME/.config/uzbl/cookies)

# MAYBE TODO: allow user to edit cookie before saving. this cannot be done with zenity :(
# TODO: different cookie paths per config (eg per group of uzbl instances)

if [ -f /usr/share/uzbl/examples/configs/cookies ]
then
	file=/usr/share/uzbl/examples/configs/cookies
else
	file=./examples/configs/cookies #useful when developing
fi

if [ -d $XDG_DATA_HOME/uzbl/cookies ]
then
	cookie_dir=$XDG_DATA_HOME/uzbl/cookies
else
	cookie_dir=./examples/data
fi

which zenity &>/dev/null || exit 2

uri=$6
action=$8 # GET/PUT
host=${uri/\/*/}



# $1 = section (TRUSTED or DENY)
# $2 =url
function match () {
	sed -n "/$1/,/^\$/p" $file 2>/dev/null | grep -q "^$host"
}

function readcookie () {
	cookie=
	while read
	do
		cookie="$REPLY
"
	done
}

function fetch_cookie () {
	cookie=`cat $cookie_dir/$host.cookie`
}

function store_cookie () {
	echo $cookie > $cookie_dir/$host.cookie
}

if match TRUSTED $host
then
	[ $action == PUT ] && readcookie && store_cookie $host
	[ $action == GET ] && fetch_cookie && echo "$cookie"
elif ! match DENY $host
then
	[ $action == PUT ] && readcookie && zenity --question --title 'Uzbl Cookie handler' --text "Accept cookie from $host ? Contents:\n$cookie" && store_cookie $host
	[ $action == GET ] && fetch_cookie && zenity --question --title 'Uzbl Cookie handler' --text "Submit cookie to $host ? Contents:\n$cookie" && echo $cookie
fi
exit 0
