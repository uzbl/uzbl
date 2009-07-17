#!/bin/sh
# use this script to pipe any variable to xclip, so you have it in your clipboard
# in your uzbl config, make the first argument the number of the (later) argument you want to use (see README for list of args)
# make the 2nd argument one of : primary, secondary, clipboard.
# examples:
# bind    yurl      = spawn ./examples/scripts/yank.sh 6 primary
# bind    ytitle    = spawn ./examples/scripts/yank.sh 7 clipboard

clip=xclip

which $clip &>/dev/null || exit 1
[ "x$9" = xprimary -o "x$9" = xsecondary -o "x$9" = xclipboard ] || exit 2

value=`eval "echo -n \\${$8}"` # bash: value = ${!8}

echo "echo -n '${value}' | $clip -selection $9"
echo -n "'${value}' | $clip -selection $9"
