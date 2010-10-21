#!/usr/bin/env python
# Per-site settings plugin

# Example configuration usage:
#
#    @on_event LOAD_COMMIT spawn @scripts_dir/per-site-settings.py $XDG_DATA_DIR/per-site-settings

# Format of the settings file:
#
#   <url>
#       <path>
#           <command>
#
# - url
#     May either be a regex, or literal. If literal, it will block any
#     subdomains as well.
# - path
#     May either be a regex, or literal. If literal, it will block any
#     decendent paths as well.
# - command
#     Given to uzbl verbatim.
#
# Matches are attempted on a literal match first.
#
# Any of the specifications can be repeated and acts as a fall-through to the
# next level. Make sure indentation lines up locally. Any indentation addition
# is considered as a fall through to the next level and any decrease is
# considered a pop back (zero is always urls). This works because it's only 3
# deep. Four and we'd have to keep track of things.

import os
import re
import socket
import stat
import subprocess
import tempfile
import urlparse
import sys


def match_url(url, patt):
    return url.endswith(patt) or re.match(patt, url)


def match_path(path, patt):
    return path.startswith(patt) or re.match(patt, path)


def grep_url(url, path, fin):
    entries = []
    cur_indent = 0
    passing = [False, False]
    # 0 == url
    # 1 == path
    # 2 == command
    state = 0
    for line in fin:
        raw = line.lstrip()
        indent = len(line) - len(raw)
        if not indent:
            # Reset state
            passing = [False, False]
            state = 0
            cur_indent = 0
        else:
            # previous level
            if indent < cur_indent:
                if state == 1:
                    passing[0] = False
                elif state == 2:
                    passing[1] = False
                state -= 1
            # next level
            if cur_indent < indent:
                state += 1

            # parse the line
            if state == 0:
                if not passing[0] and match_url(url, raw):
                    passing[0] = True
            elif state == 1 and passing[0]:
                if not passing[1] and match_path(path, raw):
                    passing[1] = True
            elif state == 2 and passing[1]:
                entries.append(raw)
        cur_indent = indent

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

    mode = os.stat(filepath)[stat.ST_MODE]

    if mode & stat.S_IEXEC:
        fin = tempfile.TemporaryFile()
        subprocess.Popen([filepath], stdout=fin).wait()
    else
        fin = open(filepath, 'r')

    host, path = (url.hostname, url.path)

    commands = grep_url(host, path, fin)

    find.close()

    write_to_socket(commands, sockpath)
