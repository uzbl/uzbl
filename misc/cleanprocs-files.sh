#!/bin/bash
killall uzbl; killall strace;
killall -9 uzbl; killall -9 strace

rm -rf /tmp/uzbl_*

echo "Uzbl processes:"
ps aux | grep uzbl | grep -v grep
echo "Uzbl /tmp entries:"
ls -alh /tmp/uzbl* 2>/dev/null
