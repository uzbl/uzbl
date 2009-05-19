#!/bin/bash

# Very simple session manager for uzbl.  When called with "endsession" as the
# argument, it'lla empty the sessionfile, look for fifos in $fifodir and
# instruct each of them to store their current url in $sessionfile and
# terminate themselves.  Run with "launch" as the argument and an instance of
# uzbl will be launched for each stored url.  "endinstance" is used internally
# and doesn't need to be called manually at any point.


scriptfile=$XDG_CONFIG_HOME/uzbl/session.sh # this script
sessionfile=$XDG_DATA_HOME/uzbl/session # the file in which the "session" (i.e. urls) are stored
UZBL="uzbl" # add custom flags and whatever here.

fifodir=/tmp # remember to change this if you instructed uzbl to put its fifos elsewhere
thisfifo="$4"
act="$8"
url="$6"

case $act in
  "launch" )
    for url in $(cat $sessionfile); do
      $UZBL --uri "$url" &
    done
    exit 0;;
  "endinstance" )
    if [ "$url" != "(null)" ]; then
      echo "$url" >> $sessionfile;
    fi
    echo "exit" > "$thisfifo"
    ;;
  "endsession" )
    echo -n "" > "$sessionfile"
    for fifo in $fifodir/uzbl_fifo_*; do
      if [ "$fifo" != "$thisfifo" ]; then
        echo "spawn $scriptfile endinstance" > "$fifo"
      fi
    done
    echo "spawn $scriptfile endinstance" > "$thisfifo";;
  * ) echo "session manager: bad action";;
esac

