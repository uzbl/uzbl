#!/bin/bash
echo "Run this test more then once. the first read on the file may be uncached. after that, the file is in Linux' block cache"

echo "Plain awk '{print \$3}':"
time awk '{print $3}' dummy_history_file >/dev/null

echo "awk + sort"
time awk '{print $3}' dummy_history_file | sort >/dev/null
echo "awk + sort + uniq"
time awk '{print $3}' dummy_history_file | sort | uniq >/dev/null

echo "Plain dmenu:"
dmenu < dummy_history_file
echo "awked into dmenu:"
awk '{print $3}' dummy_history_file | dmenu
echo "awk + sort + uniq into dmenu:"
awk '{print $3}' dummy_history_file | sort | uniq | dmenu
