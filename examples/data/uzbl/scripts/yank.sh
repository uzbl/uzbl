# use this script to pipe any variable to xclip, so you have it in your clipboard
# in your uzbl config, make the first argument the number of the (later) argument you want to use (see README for list of args)
# make the 2nd argument one of : primary, secondary, clipboard.
# examples:
# bind    yurl      = spawn ./examples/scripts/yank.sh 6 primary
# bind    ytitle    = spawn ./examples/scripts/yank.sh 7 clipboard

which xclip &>/dev/null || exit 1
[ "$9" == primary -o "$9" == secondary -o "$9" == clipboard ] || exit 2

echo echo -n "${!8}" '|' xclip -selection $9
echo -n "${!8}" | xclip -selection $9
