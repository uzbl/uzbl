#!/bin/sh

set -n;

# THIS IS EXPERIMENTAL AND COULD BE INSECURE !!!!!!

# this is an example bash script of how you could manage your cookies. it is very raw and basic and not as good as cookies.py
# we use the cookies.txt format (See http://kb.mozillazine.org/Cookies.txt)
# This is one textfile with entries like this:
# kb.mozillazine.org	FALSE	/	FALSE	1146030396	wikiUserID	16993
# domain alow-read-other-subdomains path http-required expiration name value
# you probably want your cookies config file in your $XDG_CONFIG_HOME ( eg $HOME/.config/uzbl/cookies)
# Note. in uzbl there is no strict definition on what a session is.  it's YOUR job to clear cookies marked as end_session if you want to keep cookies only valid during a "session"
# MAYBE TODO: allow user to edit cookie before saving. this cannot be done with zenity :(
# TODO: different cookie paths per config (eg per group of uzbl instances)

# TODO: correct implementation.
# see http://curl.haxx.se/rfc/cookie_spec.html
# http://en.wikipedia.org/wiki/HTTP_cookie

# TODO : check expires= before sending.
# write sample script that cleans up cookies dir based on expires attribute.
# TODO: check uri against domain attribute. and path also.
# implement secure attribute.
# support blocking or not for 3rd parties
# http://kb.mozillazine.org/Cookies.txt
# don't always append cookies, sometimes we need to overwrite

cookie_config=${XDG_CONFIG_HOME:-${HOME}/.config}/uzbl/cookies
[ "x$cookie_config" = x ] && exit 1
[ -d "${XDG_DATA_HOME:-${HOME}/.local/share}/uzbl/" ] &&\
cookie_data=${XDG_DATA_HOME:-${HOME}/.local/share}/uzbl/cookies.txt || exit 1

notifier=
#notifier=notify-send
#notify_wrapper () {
#	echo "$@" >> $HOME/cookielog
#}
#notifier=notifier_wrapper

# if this variable is set, we will use it to inform you when and which cookies we store, and when/which we send.
# it's primarily used for debugging
notifier=
which zenity &>/dev/null || exit 2

# Example cookie:
# test_cookie=CheckForPermission; expires=Thu, 07-May-2009 19:17:55 GMT; path=/; domain=.doubleclick.net

# uri=$6
# uri=${uri/http:\/\/} # strip 'http://' part
# host=${uri/\/*/}
action=$8 # GET/PUT
shift
host=$9
shift
path=$9
shift
cookie=$9

field_domain=$host
field_path=$path
field_name=
field_value=
field_exp='end_session'

notify() {
	[ -n "$notifier" ] && $notifier "$@"
}


# FOR NOW LETS KEEP IT SIMPLE AND JUST ALWAYS PUT AND ALWAYS GET
parse_cookie() {
	IFS=$';'
	first_pair=1
	for pair in $cookie
	do
		if [ "x$first_pair" = x1 ]
		then
			field_name=${pair%%=*}
			field_value=${pair#*=}
			first_pair=0
		else
			echo "$pair" | read -r pair #strip leading/trailing wite space
			key=${pair%%=*}
			val=${pair#*=}
			[ "$key" == expires ] && field_exp=`date -u -d "$val" +'%s'`
			# TODO: domain
			[ "$key" == path ] && field_path=$val
		fi
	done
	unset IFS
}

# match cookies in cookies.txt against hostname and path
get_cookie() {
	path_esc=${path//\//\\/}
	search="^[^\t]*$host\t[^\t]*\t$path_esc"
	cookie=`awk "/$search/" $cookie_data 2>/dev/null | tail -n 1`
	if [ -z "$cookie" ]
	then
		notify "Get_cookie: search: $search in $cookie_data -> no result"
		false
	else
		notify "Get_cookie: search: $search in $cookie_data -> result: $cookie"
		echo "$cookie" | \
      read domain alow_read_other_subdomains path http_required expiration name \
        value;
		cookie="$name=$value"
		true
	fi
}

save_cookie() {
	if parse_cookie
	then
		data="$field_domain\tFALSE\t$field_path\tFALSE\t$field_exp\t$field_name\t$field_value"
		notify "save_cookie: adding $data to $cookie_data"
		echo -e "$data" >> $cookie_data
	else
		notify "not saving a cookie. since we don't have policies yet, parse_cookie must have returned false. this is a bug"
	fi
}

[ "x$action" = xPUT ] && save_cookie
[ "x$action" = xGET ] && get_cookie && echo "$cookie"

exit


# TODO: implement this later.
# $1 = section (TRUSTED or DENY)
# $2 =url
match() {
	sed -n "/$1/,/^\$/p" $cookie_config 2>/dev/null | grep -q "^$host"
}

fetch_cookie() {
	cookie=`cat $cookie_data`
}

store_cookie() {
	echo $cookie > $cookie_data
}

if match TRUSTED $host
then
	[ "x$action" = xPUT ] && store_cookie $host
	[ "x$action" = xGET ] && fetch_cookie && echo "$cookie"
elif ! match DENY $host
then
	[ "x$action" = xPUT ] &&                 cookie=`zenity --entry --title 'Uzbl Cookie handler' --text "Accept this cookie from $host ?" --entry-text="$cookie"` && store_cookie $host
	[ "x$action" = xGET ] && fetch_cookie && cookie=`zenity --entry --title 'Uzbl Cookie handler' --text "Submit this cookie to $host ?"   --entry-text="$cookie"` && echo $cookie
fi
exit 0
