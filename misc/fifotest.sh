#!/bin/bash
if [ -z "$1" ]
then
	echo "Need fifo filename!" >&2
	exit 2
fi
while :
do
	echo 'uri dns.be'
	echo 'uri dns.be' > $1
	sleep 2
	echo 'uri www.archlinux.org'
	echo 'uri www.archlinux.org' > $1
	sleep 2
	echo 'uri icanhascheezburger.com'
	echo 'uri icanhascheezburger.com' > $1
	sleep 2
	echo 'back'
	echo 'back' > $1
done
