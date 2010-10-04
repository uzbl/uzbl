#!/usr/bin/env python
# Per-site settings plugin

# Use like:
#
#    @on_event LOAD_COMMIT spawn @scripts_dir/per-site-settings.py $XDG_DATA_DIR/per-site-settings

# Format of the settings file:
#
#   <url><TAB><path><TAB><command>
#
# - url
#     May either be a regex, or literal. If literal, it will block any
#     subdomains as well.
# - path
#     May either be a regex, or literal. If literal, it will block any
#     decendent paths as well.
# - options
#     Given to uzbl verbatim.

import os
import re
import socket
import urlparse
import sys

def grep_url(url, path, filepath):
    entries = []
    with open(filepath, 'r') as f:
        for line in f:
            parts = line.split('\t', 2)
            if (url.endswith(parts[0]) or re.match(parts[0], url)) and \
               (path.startswith(parts[1]) or re.match(parts[1], path)):
                entries.append(parts[2])
    return entries

def write_to_socket(commands, sockpath):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(sockpath)
    for command in commands:
        sock.write(command)
    sock.close()

if __name__ == '__main__':
    sockpath = sys.argv[5]
    url = urlparse.urlparse(sys.argv[6])
    filepath = sys.argv[8]

    host, path = (url.hostname, url.path)

    commands = grep_url(host, path, filepath)

    write_to_socket(commands, sockpath)
