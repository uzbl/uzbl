# use this script to pipe any variable to xclip, so you have it in your clipboard
exit
# this script is not done yet

# can we make this universal?
# add xclip to optdeps

which xclip &>/dev/null || exit 1

echo -n `eval "$3"` #| xclip