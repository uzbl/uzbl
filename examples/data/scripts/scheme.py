#!/usr/bin/env python

import os
import subprocess
import sys

try:
  import urllib.parse as urlparse
except ImportError:
  import urlparse


def detach_open(cmd, **kwargs):
    # Thanks to the vast knowledge of Laurence Withers (lwithers) and this message:
    # http://mail.python.org/pipermail/python-list/2006-November/587523.html
    if not os.fork():
        null = os.open(os.devnull, os.O_WRONLY)
        for i in range(3):
            os.dup2(null, i)
        os.close(null)
        subprocess.Popen(cmd, **kwargs)
    print('USED')


def main(uri):
    u = urlparse.urlparse(uri)

    if u.scheme == 'mailto':
        detach_open(['xterm', '-e', 'mail', u.path])
    elif u.scheme == 'xmpp':
        # Someone check for safe arguments to gajim-remote
        detach_open(['gajim-remote', 'open_chat', uri])
    elif u.scheme == 'git':
        detach_open(['git', 'clone', '--', uri], cwd=os.path.expanduser('~/src'))


if __name__ == '__main__':
    try:
        uri = sys.argv[1]
    except IndexError:
        print('Error: No uri given to handle.')

        os.exit(1)

    main(uri)
