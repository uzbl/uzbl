#!/bin/sh
# it would be better to use something more flexible like $(dirname $0) but i
# couldn't get to work nicely from the Makefile:
# - sourcing file gives $0 == /bin/sh
# - executing limits scope of variables too much (even with exporting)
# maybe we should spawn processes from here with an 'exec' at the end?

XDG_DATA_HOME=./sandbox/examples/data
XDG_CACHE_HOME=./sandbox/examples/cache
XDG_CONFIG_HOME=./sandbox/examples/config
PATH=".sandbox/usr/local/share/uzbl/examples/data/uzbl/scripts/:$PATH" # needed when running uzbl-browser from here. not sure anymore why
PATH="./sandbox/usr/local/bin:$PATH" # needed to run uzbl-browser etc from here
