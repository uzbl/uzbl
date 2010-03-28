#!/bin/sh

config=$1;
shift
pid=$1;
shift
xid=$1;
shift
fifo=$1;
shift
socket=$1;
shift
url=$1;
shift
title=$1;
shift

echo 'script @scripts_dir/follow.js "@{follow_hint_keys} '$1'"' | socat - unix-connect:$socket

# Read variable set by script. If it equals XXXFORMELEMENTCLICKEDXXX emit
# FORM_ACTIVE event
echo 'js rv' \
  | socat - unix-connect:$socket \
  | grep -q XXXFORMELEMENTCLICKEDXXX \
  && echo 'event FORM_ACTIVE' \
  | socat - unix-connect:$socket
