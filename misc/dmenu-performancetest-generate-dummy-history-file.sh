#!/bin/bash
echo "Creating dummy file of 50MB in size (625000 entries of 80chars)"
echo "Note: this takes about an hour and a half"
entries_per_iteration=1000
for i in `seq 1 625`
do
	echo "Iteration $i of 625 ( $entries_per_iteration each )"
	for j in `seq 1 $entries_per_iteration`
	do
		echo "`date +'%Y-%m-%d %H:%M:%S'` `date +%s`abcdefhijklmno`date +%s | md5sum`" >> ./dummy_history_file
	done
done
