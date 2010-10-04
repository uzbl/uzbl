#!/usr/bin/env python
# Per-site settings plugin

# Format of the noscript file:
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

def noscript_path():
    env = os.environ
    if 'XDG_DATA_HOME' in env:
        root = env['XDG_DATA_HOME']
    else:
        root = os.path.join(env['HOME'], '.local', 'share')
    return os.path.join(root, 'uzbl', 'per-site-options')

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

    host, path = (url.hostname, url.path)

    commands = grep_url(host, path, noscript_path())

    write_to_socket(commands, sockpath)
