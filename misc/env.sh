#!/bin/sh
# This script is copied in to the sandbox/ directory during any sandbox make
# target and is used to create an isolated environment for uzbl-core,
# uzbl-browser and uzbl-tabbed to be run and tested.

# It would be better to use something more flexible like $(dirname $0) but I
# couldn't get to work nicely from the Makefile:
#  - Sourcing file gives $0 == /bin/sh
#  - Executing limits scope of variables too much (even with exporting)
# Maybe we should spawn processes from here with an 'exec' at the end?

# Re-define our home location inside the sandbox dir.
export HOME=$(pwd)/sandbox/home

# Export default XDG_{DATA,CACHE,..}_HOME locations inside the sandbox
# directory according to defaults in the xdg specification.
# <http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html>
export XDG_DATA_HOME=$HOME/.local/share
export XDG_CACHE_HOME=$HOME/.cache
export XDG_CONFIG_HOME=$HOME/.config

# Needed to run uzbl-browser etc from here.
export PATH="$(pwd)/sandbox/usr/local/bin:$PATH"
