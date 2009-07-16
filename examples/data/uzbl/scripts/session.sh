#!/bin/sh

# Very simple session manager for uzbl.  When called with "endsession" as the
# argument, it'll backup $sessionfile, look for fifos in $fifodir and
# instruct each of them to store their current url in $sessionfile and
# terminate themselves.  Run with "launch" as the argument and an instance of
# uzbl will be launched for each stored url.  "endinstance" is used internally
# and doesn't need to be called manually at any point.
# Add a line like 'bind quit = /path/to/session.sh endsession' to your config

[ -d ${XDG_DATA_HOME:-$HOME/.local/share}/uzbl ] || exit 1
scriptfile=$0 				# this script
sessionfile=${XDG_DATA_HOME:-$HOME/.local/share}/uzbl/session # the file in which the "session" (i.e. urls) are stored
configfile=${XDG_DATA_HOME:-$HOME/.local/share}/uzbl/config   # uzbl configuration file
UZBL="uzbl -c $configfile"           # add custom flags and whatever here.

fifodir=/tmp # remember to change this if you instructed uzbl to put its fifos elsewhere
thisfifo="$4"
act="$8"
url="$6"

if [ "$act." = "." ]; then
   act="$1"
fi


case $act in
  "launch" )
    urls=`cat $sessionfile`
    if [ "$urls." = "." ]; then
      $UZBL
    else
      for url in $urls; do
        $UZBL --uri "$url" &
      done
    fi
    exit 0
    ;;

  "endinstance" )
    if [ "$url" != "(null)" ]; then
      echo "$url" >> $sessionfile;
    fi
    echo "exit" > "$thisfifo"
    ;;

  "endsession" )
    mv "$sessionfile" "$sessionfile~"
    for fifo in $fifodir/uzbl_fifo_*; do
      if [ "$fifo" != "$thisfifo" ]; then
        echo "spawn $scriptfile endinstance" > "$fifo"
      fi
    done
    echo "spawn $scriptfile endinstance" > "$thisfifo"
    ;;

  * ) echo "session manager: bad action"
      echo "Usage: $scriptfile [COMMAND] where commands are:"
      echo " launch 	- Restore a saved session or start a new one"
      echo " endsession	- Quit the running session. Must be called from uzbl"
      ;;
esac
