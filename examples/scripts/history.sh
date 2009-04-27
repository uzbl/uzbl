#!/bin/bash
#TODO: strip 'http://' part
# you probably really want this in your $XDG_DATA_HOME (eg $HOME/.local/share/uzbl/history)
echo "$7 $5" >> /tmp/uzbl.history
