#!/usr/bin/env python

import StringIO, cookielib, os, sys, urllib2

if __name__ == '__main__':
    action = sys.argv[8]
    uri = urllib2.urlparse.ParseResult(
            scheme=sys.argv[9],
            netloc=sys.argv[10],
            path=sys.argv[11],
            params='',
            query='',
            fragment='').geturl()
    set_cookie = sys.argv[12] if len(sys.argv)>12 else None

    if 'XDG_DATA_HOME' in os.environ.keys() and os.environ['XDG_DATA_HOME']:
        f = os.path.join(os.environ['XDG_DATA_HOME'],'uzbl/cookies.txt')
    else:
        f = os.path.join(os.environ['HOME'],'.local/share/uzbl/cookies.txt')
    jar = cookielib.MozillaCookieJar(f)

    try:
        jar.load(ignore_discard=True)
    except:
        pass

    req = urllib2.Request(uri)

    if action == 'GET':
        jar.add_cookie_header(req)
        if req.has_header('Cookie'):
            print req.get_header('Cookie')
    elif action == 'PUT':
        hdr = urllib2.httplib.HTTPMessage(StringIO.StringIO('Set-Cookie: %s' % set_cookie))
        res = urllib2.addinfourl(StringIO.StringIO(), hdr, req.get_full_url())
        jar.extract_cookies(res,req)
        jar.save(ignore_discard=True)
