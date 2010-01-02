#!/bin/sh
# it would be better to use something more flexible like $(dirname $0) but i
# couldn't get to work nicely from the Makefile:
# - sourcing file gives $0 == /bin/sh
# - executing limits scope of variables too much (even with exporting)
# maybe we should spawn processes from here with an 'exec' at the end?

export HOME=./sandbox/home
export XDG_DATA_HOME=$HOME/.local/share
export XDG_CACHE_HOME=$HOME/.cache
export XDG_CONFIG_HOME=$HOME/.config
#export PATH="./sandbox/usr/local/share/uzbl/examples/data/scripts/:$PATH" # needed when running uzbl-browser from here? don't think so..
export PATH="./sandbox/usr/local/bin:$PATH" # needed to run uzbl-browser etc from here
