#!/usr/bin/env python

import cookielib, sys, os, urllib2

class FakeRequest:
    def __init__(self, argv):
        self.argv = argv
        self.cookies = None
        if len(self.argv) == 12:
            self.cookies = self.argv[11]
    def get_full_url(self):
        #TODO: this is a hack, fix in uzbl.c!
        u = self.get_host()+self.argv[10]
        if self.argv[6].startswith('https'):
            u = 'https://'+u
        else:
            u = 'http://'+u
        return u
    def get_host(self):
        return self.argv[9]
    def get_type(self):
        return self.get_full_url().split(':')[0]
    def is_unverifiable(self):
        return False
    def get_origin_req_host(self):
        return self.argv[9]
    def has_header(self, header):
        if header == 'Cookie':
            return self.cookies!=None
    def get_header(self, header_name, default=None):
        if header_name == 'Cookie' and self.cookies:
            return self.cookies
        else:
            return default
    def header_items(self):
        if self.cookies:
            return [('Cookie',self.cookies)]
        else:
            return []
    def add_unredirected_header(self, key, header):
        if key == 'Cookie':
            self.cookies = header

class FakeHeaders:
    def __init__(self, argv):
        self.argv = argv
    def getallmatchingheaders(self, header):
        if header == 'Set-Cookie' and len(self.argv) == 12:
            return ['Set-Cookie: '+self.argv[11]]
        else:
            return []
    def getheaders(self, header):
        if header == 'Set-Cookie' and len(self.argv) == 12:
            return [self.argv[11]]
        else:
            return []
class FakeResponse:
    def __init__(self, argv):
        self.argv = argv
    def info(self):
        return FakeHeaders(self.argv)

if __name__ == '__main__':
    if 'XDG_DATA_HOME' in os.environ.keys() and os.environ['XDG_DATA_HOME']:
        jar = cookielib.MozillaCookieJar(
          os.path.join(os.environ['XDG_DATA_HOME'],'uzbl/cookies.txt'))
    else:
        jar = cookielib.MozillaCookieJar(
          os.path.join(os.environ['HOME'],'.local/share/uzbl/cookies.txt'))
    try:
        jar.load()
    except:
        pass

    req = FakeRequest(sys.argv)

    action = sys.argv[8]

    if action == 'GET':
        jar.add_cookie_header(req)
        if req.cookies:
            print req.cookies
    elif action == 'PUT':
        res = FakeResponse(sys.argv)
        jar.extract_cookies(res,req)
        jar.save(ignore_discard=True) # save session cookies too
        #jar.save() # save everything but session cookies
