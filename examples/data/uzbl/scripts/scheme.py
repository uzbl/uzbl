#!/usr/bin/env python

import os, subprocess, sys, urlparse

if __name__ == '__main__':
    uri = sys.argv[8]
    u = urlparse.urlparse(uri)
    if u.scheme == 'mailto':
        subprocess.call(['xterm', '-e', 'mail %s' % u.path])
    elif u.scheme == 'xmpp':
        subprocess.call(['gajim-remote', 'open_chat', uri])
    #elif u.scheme == 'git':
        #os.chdir(os.path.expanduser('~/src'))
        #subprocess.call(['git', 'clone', uri])
