#!/usr/bin/env python
#
#       filename
#
#       Copyright 2010 John Tyree <johntyree@gmail.com>
#
#       This program is free software; you can redistribute it and/or modify
#       it under the terms of the GNU General Public License as published by
#       the Free Software Foundation; either version 3 of the License, or
#       (at your option) any later version.
#
#       This program is distributed in the hope that it will be useful,
#       but WITHOUT ANY WARRANTY; without even the implied warranty of
#       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#       GNU General Public License for more details.
#
#       You should have received a copy of the GNU General Public License
#       along with this program; if not, write to the Free Software
#       Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#       MA 02110-1301, USA.

import sys
import urllib2

# {0} is replaced with the uri that is passed to rewrite()
rules = {
    'g'  : 'http://www.google.com/search?q={0}',
    'ddg': 'http://duckduckgo.com/?q={0}',
    'awp': 'http://wiki.archlinux.org/index.php/Special:Search?search={0}&go=Go',
    'wp' : 'http://en.wikipedia.org/w/index.php?title=Special:Search&search={0}&go=Go',
    'h'  : 'http://www.haskell.org/hoogle/?hoogle={0}',
}

def rewrite(uri):
    if ' ' in uri:
        # Parse and sanitize
        [rule, _, uri] = [x.strip() for x in uri.partition(' ')]
        uri = rules[rule].format(urllib2.quote(uri))
    return uri


def main():
    for urlish in sys.stdin:
        print rewrite(urlish)
    return 0

if __name__ == '__main__': main()
