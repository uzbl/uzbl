""" Basic cookie manager
    forwards cookies to all other instances connected to the event manager"""

from collections import defaultdict
import os
import re
import stat

from uzbl.arguments import splitquoted
from uzbl.ext import GlobalPlugin, PerInstancePlugin
from uzbl.xdg import xdg_data_home

# these are symbolic names for the components of the cookie tuple
symbolic = {
    'domain': 0,
    'path': 1,
    'name': 2,
    'value': 3,
    'scheme': 4,
    'expires': 5
}


# allows for partial cookies
# ? allow wildcard in key
def match(key, cookie):
    for k, c in zip(key, cookie):
        if k != c:
            return False
    return True


def match_list(_list, cookie):
    for matcher in _list:
        for component, match in matcher:
            if match(cookie[component]) is None:
                break
        else:
            return True
    return False


def add_cookie_matcher(_list, arg):
    ''' add a cookie matcher to a whitelist or a blacklist.
        a matcher is a list of (component, re) tuples that matches a cookie
        when the "component" part of the cookie matches the regular expression
        "re". "component" is one of the keys defined in the variable
        "symbolic" above, or the index of a component of a cookie tuple.
    '''

    args = splitquoted(arg)
    mlist = []
    for (component, regexp) in zip(args[0::2], args[1::2]):
        try:
            component = symbolic[component]
        except KeyError:
            component = int(component)
        assert component <= 5
        mlist.append((component, re.compile(regexp).search))
    _list.append(mlist)


class NullStore(object):
    def __init__(self, filename):
        super(NullStore, self).__init__()

    def add_cookie(self, rawcookie, cookie):
        pass

    def delete_cookie(self, rkey, key):
        pass


class ListStore(list):
    def __init__(self, filename):
        super(ListStore, self).__init__()

    def add_cookie(self, rawcookie, cookie):
        self.append(rawcookie)

    def delete_cookie(self, rkey, key):
        self[:] = [x for x in self if not match(key, splitquoted(x))]


class TextStore(object):
    def __init__(self, filename):
        self.filename = filename
        try:
            # make sure existing cookie jar is not world-open
            perm_mode = os.stat(self.filename).st_mode
            world_mode = (stat.S_IRWXO | stat.S_IRWXG)
            if (perm_mode & world_mode) > 0:
                safe_perm = stat.S_IMODE(perm_mode) & ~world_mode
                os.chmod(self.filename, safe_perm)
        except OSError:
            pass

    def as_event(self, cookie):
        """Convert cookie.txt row to uzbls cookie event format"""
        scheme = {
            'TRUE': 'https',
            'FALSE': 'http'
        }
        extra = ''
        if cookie[0].startswith("#HttpOnly_"):
            extra = 'Only'
            domain = cookie[0][len("#HttpOnly_"):]
        elif cookie[0].startswith('#'):
            return None
        else:
            domain = cookie[0]
        try:
            return (domain,
                    cookie[2],
                    cookie[5],
                    cookie[6],
                    scheme[cookie[3]] + extra,
                    cookie[4])
        except (KeyError, IndexError):
            # Let malformed rows pass through like comments
            return None

    def as_file(self, cookie):
        """Convert cookie event to cookie.txt row"""
        secure = {
            'https': 'TRUE',
            'http': 'FALSE',
            'httpsOnly': 'TRUE',
            'httpOnly': 'FALSE'
        }
        http_only = {
            'https': '',
            'http': '',
            'httpsOnly': '#HttpOnly_',
            'httpOnly': '#HttpOnly_'
        }

        return (http_only[cookie[4]] + cookie[0],
                'TRUE' if cookie[0].startswith('.') else 'FALSE',
                cookie[1],
                secure[cookie[4]],
                cookie[5],
                cookie[2],
                cookie[3])

    def add_cookie(self, rawcookie, cookie):
        assert len(cookie) == 6

        # delete equal cookies (ignoring expire time, value and secure flag)
        self.delete_cookie(None, cookie[:-3])

        # restrict umask before creating the cookie jar
        curmask = os.umask(0)
        os.umask(curmask | stat.S_IRWXO | stat.S_IRWXG)

        first = not os.path.exists(self.filename)
        with open(self.filename, 'a') as f:
            if first:
                print("# HTTP Cookie File", file=f)
            print('\t'.join(self.as_file(cookie)), file=f)

    def delete_cookie(self, rkey, key):
        if not os.path.exists(self.filename):
            return

        # restrict umask before creating the cookie jar
        curmask = os.umask(0)
        os.umask(curmask | stat.S_IRWXO | stat.S_IRWXG)

        # read all cookies
        with open(self.filename, 'r') as f:
            cookies = f.readlines()

        # write those that don't match the cookie to delete
        with open(self.filename, 'w') as f:
            for l in cookies:
                c = self.as_event(l.split('\t'))
                if c is None or not match(key, c):
                    print(l, end='', file=f)
        os.umask(curmask)


DEFAULT_STORE = None
SESSION_STORE = None

STORES = {
    'text': TextStore,
    'memory': ListStore,
    'null': NullStore,
}


class Cookies(PerInstancePlugin):
    CONFIG_SECTION = 'cookies'

    def __init__(self, uzbl):
        super(Cookies, self).__init__(uzbl)

        self.secure = []
        self.whitelist = []
        self.blacklist = []

        uzbl.connect('ADD_COOKIE', self.add_cookie)
        uzbl.connect('DELETE_COOKIE', self.delete_cookie)
        uzbl.connect('BLACKLIST_COOKIE', self.blacklist_cookie)
        uzbl.connect('WHITELIST_COOKIE', self.whitelist_cookie)

        # HTTPS-Everywhere support
        uzbl.connect('SECURE_COOKIE', self.secure_cookie)
        uzbl.connect('CLEAR_SECURE_COOKIE_RULES', self.clear_secure_cookies)

    # accept a cookie only when one of the following is true:
    # a. there is no whitelist and the cookie is not in the blacklist
    # b. the cookie is in the whitelist and not in the blacklist
    def accept_cookie(self, cookie):
        if self.whitelist:
            if match_list(self.whitelist, cookie):
                return not match_list(self.blacklist, cookie)
            return False

        return not match_list(self.blacklist, cookie)

    def expires_with_session(self, cookie):
        return cookie[5] == ''

    def get_recipents(self):
        """ get a list of Uzbl instances to send the cookie too. """
        # This could be a lot more interesting
        # TODO(mathstuf): respect private browsing mode.
        uzbls = self.uzbl.parent.uzbls.values()
        return [u for u in uzbls if u is not self.uzbl]

    def _make_store(self, cookie_type, envvar, fname):
        store_type = self.plugin_config.get('%s.type' % cookie_type, 'text')
        if store_type not in STORES:
            self.logger.error('cookies: unknown store type: %s' % store_type)
            store_type = 'memory'
        store = STORES[store_type]

        try:
            path = os.environ[envvar]
        except KeyError:
            default_path = os.path.join(xdg_data_home, 'uzbl', fname)
            path = self.plugin_config.get('%s.path' % cookie_type,
                                          default_path)

        return store(path)

    def get_store(self, session=False):
        global SESSION_STORE
        global DEFAULT_STORE

        if session:
            if SESSION_STORE is None:
                SESSION_STORE = self._make_store('session',
                                                 'UZBL_SESSION_COOKIE_FILE',
                                                 'session-cookies.txt')
            return SESSION_STORE

        if DEFAULT_STORE is None:
            DEFAULT_STORE = self._make_store('global',
                                             'UZBL_COOKIE_FILE',
                                             'cookies.txt')
        return DEFAULT_STORE

    def add_cookie(self, cookie):
        cookie = splitquoted(cookie)

        if self.secure:
            if match_list(self.secure, cookie):
                make_secure = {
                    'http': 'https',
                    'httpOnly': 'httpsOnly'
                }
                if cookie[4] in make_secure:
                    self.uzbl.send('cookie delete %s' % cookie.safe_raw())

                    new_cookie = list(cookie)
                    new_cookie[4] = make_secure[cookie[4]]
                    new_cookie = tuple(new_cookie)

                    self.uzbl.send('cookie add %s' % new_cookie.safe_raw())
                    return

        if self.accept_cookie(cookie):
            for u in self.get_recipents():
                u.send('cookie add %s' % cookie.safe_raw())

            store = self.get_store(self.expires_with_session(cookie))
            store.add_cookie(cookie.raw(), cookie)
        else:
            self.logger.debug('cookie %r is blacklisted', cookie)
            self.uzbl.send('cookie delete %s' % cookie.safe_raw())

    def delete_cookie(self, cookie):
        cookie = splitquoted(cookie)
        for u in self.get_recipents():
            u.send('cookie delete %s' % cookie.safe_raw())

        if len(cookie) == 6:
            store = self.get_store(self.expires_with_session(cookie))
            store.delete_cookie(cookie.raw(), cookie)
        else:
            stores = set(self.get_store(session) for session in (True, False))
            for store in stores:
                store.delete_cookie(cookie.raw(), cookie)

    def blacklist_cookie(self, arg):
        add_cookie_matcher(self.blacklist, arg)

    def whitelist_cookie(self, arg):
        add_cookie_matcher(self.whitelist, arg)

    def secure_cookie(self, arg):
        add_cookie_matcher(self.secure, arg)

    def clear_secure_cookies(self, arg):
        self.secure = []
