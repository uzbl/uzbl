#!/usr/bin/env python2

import os, subprocess, sys, urlparse

def detach_open(cmd):
    # Thanks to the vast knowledge of Laurence Withers (lwithers) and this message:
    # http://mail.python.org/pipermail/python-list/2006-November/587523.html
    if not os.fork():
        null = os.open(os.devnull,os.O_WRONLY)
        for i in range(3): os.dup2(null,i)
        os.close(null)
        subprocess.Popen(cmd)
    print 'USED'

if __name__ == '__main__':
    uri = sys.argv[1]
    u = urlparse.urlparse(uri)
    if u.scheme == 'mailto':
        detach_open(['xterm', '-e', 'mail', u.path])
    elif u.scheme == 'xmpp':
        # Someone check for safe arguments to gajim-remote
        detach_open(['gajim-remote', 'open_chat', uri])
    elif u.scheme == 'git':
        detach_open(['git', 'clone', '--', uri], cwd=os.path.expanduser('~/src'))
