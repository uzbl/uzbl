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

# Read variable set by script. If it equals XXXEMIT_FORM_ACTIVEXXX emit
# FORM_ACTIVE event. If it equals XXXEMIT_ROOT_ACTIVEXXX emit ROOT_ACTIVE
# event.
rv=$(echo 'js rv' | socat - unix-connect:$socket)

echo $rv \
  | grep -q XXXEMIT_FORM_ACTIVEXXX \
  && echo 'event FORM_ACTIVE' \
  | socat - unix-connect:$socket

echo $rv \
  | grep -q XXXRESET_MODEXXX \
  && echo 'set mode =' \
  | socat - unix-connect:$socket
