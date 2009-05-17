# use this script to pipe any variable to xclip, so you have it in your clipboard
# in your uzbl config, make the first argument the number of the (later) argument you want to use (see README for list of args)
# make the 2nd argument one of : primary, secondary, clipboard.
# examples:
# bind    yurl      = spawn ./examples/scripts/yank.sh 8 primary
# bind    ytitle    = spawn ./examples/scripts/yank.sh 9 clipboard

which xclip &>/dev/null || exit 1
[ "$2" == primary -o "$2" == secondary -o "$2" == clipboard ] || exit 2

echo -n "${!1}" | xclip -selection $2