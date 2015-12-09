#!/usr/bin/env python2
# Per-site settings plugin

# Example configuration usage:
#
#    @on_event LOAD_COMMIT spawn @scripts_dir/per-site-settings.py @data_home/uzbl/per-site-settings

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
#     Given to uzbl verbatim. The special string '@' "breaks" to the top-level
#     (i.e., any further path matches patterns will fail). The special string
#     '@@' will stop processing of the file.
#
# Matches are attempted on a literal match first.
#
# Any of the specifications can be repeated and acts as a fall-through to the
# next level. Make sure indentation lines up locally. Any indentation addition
# is considered as a fall through to the next level and any decrease is
# considered a pop back (zero is always urls). This works because it's only 3
# deep. Four and we'd have to keep track of things.

import glob
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
        raw = line.strip()

        if raw == '@':
            passing[0] = False
            passing[1] = False
            continue
        elif raw == '@@':
            return entries

        indent = len(line) - len(raw) - 1
        if not indent and state:
            # Reset state
            passing = [False, False]
            state = 0
        else:
            # previous level
            if indent < cur_indent:
                if state == 1:
                    passing[0] = False
                elif state == 2:
                    passing[1] = False
                state -= 1
            # next level
            elif cur_indent < indent:
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
        sock.send('%s\n' % command)
    sock.close()


def main(filepath, fileglob):
    sockpath = os.environ['UZBL_SOCKET']
    url = urlparse.urlparse(os.environ['UZBL_URI'])

    if not url.hostname:
        return

    if os.path.isdir(filepath):
        fin = tempfile.TemporaryFile()

        for sett in sorted(glob.glob(os.path.join(filepath, fileglob))):
            with open(sett, 'r') as sfin:
                fin.write(sfin.read())

        fin.seek(0)
    elif os.path.isfile(filepath):
        mode = os.stat(filepath)[stat.ST_MODE]

        if mode & stat.S_IEXEC:
            fin = tempfile.TemporaryFile()
            subprocess.Popen([filepath], stdout=fin).wait()
        else:
            fin = open(filepath, 'r')
    else:
        sys.stderr.write('%s: Error: \'%s\' is neither a directory nor a file\n' % (sys.argv[0], filepath))

        sys.exit(1)

    commands = grep_url(url.hostname, url.path, fin)

    fin.close()

    write_to_socket(commands, sockpath)


if __name__ == '__main__':
    try:
        filepath = sys.argv[1]
    except IndexError:
        sys.stderr.write('%s: Error: No file given to read.\n', sys.argv[0])

        sys.exit(1)

    try:
        fileglob = sys.argv[2]
    except IndexError:
        fileglob = '*.pss'

    main(filepath, fileglob)
