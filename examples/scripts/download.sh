#!/bin/bash
# A bit more advanced, try some pattern matching on the link to determine
# a target location. pushd and popd might be better than plain cd...
if [[ $8 =~ .*(.torrent) ]] 
then
    pushd /your/place/for/torrent/files
else
    pushd /your/place/for/usual/downloads
fi
# Some sites block the default wget --user-agent...
wget --user-agent=Firefox $8
popd
