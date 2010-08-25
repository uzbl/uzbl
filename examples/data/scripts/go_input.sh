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

case $(echo 'script @scripts_dir/go_input.js' | socat - unix-connect:$socket) in
  *XXXEMIT_FORM_ACTIVEXXX*) echo 'event FORM_ACTIVE' | socat - unix-connect:$socket ;;
esac
