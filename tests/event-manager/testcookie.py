#!/usr/bin/env python


import sys
if '' not in sys.path:
    sys.path.insert(0, '')

import unittest
from emtest import EventManagerMock

from uzbl.plugins.cookies import Cookies
from uzbl.plugins.config import Config

cookies = (
    r'".nyan.cat" "/" "__utmb" "183192761.1.10.1313990640" "http" "1313992440"',
    r'".twitter.com" "/" "guest_id" "v1%3A131399064036991891" "http" "1377104460"'
)

config = {
    'cookies': {
        'session.type': 'memory',
        'global.type': 'memory'
    }
}


class CookieFilterTest(unittest.TestCase):
    def setUp(self):
        self.event_manager = EventManagerMock((), (Cookies,),
                                              plugin_config=config)
        self.uzbl = self.event_manager.add()
        self.other = self.event_manager.add()

    def test_add_cookie(self):
        c = Cookies[self.uzbl]
        c.add_cookie(cookies[0])
        self.other.send.assert_called_once_with(
            'cookie add ' + cookies[0])

    def test_whitelist_block(self):
        c = Cookies[self.uzbl]
        c.whitelist_cookie(r'domain "nyan\.cat$"')
        c.add_cookie(cookies[1])
        self.uzbl.send.assert_called_once_with(
            'cookie delete ' + cookies[1])

    def test_whitelist_accept(self):
        c = Cookies[self.uzbl]
        c.whitelist_cookie(r'domain "nyan\.cat$"')
        c.add_cookie(cookies[0])
        self.other.send.assert_called_once_with(
            'cookie add ' + cookies[0])

    def test_blacklist_block(self):
        c = Cookies[self.uzbl]
        c.blacklist_cookie(r'domain "twitter\.com$"')
        c.add_cookie(cookies[1])
        self.uzbl.send.assert_called_once_with(
            'cookie delete ' + cookies[1])

    def test_blacklist_accept(self):
        c = Cookies[self.uzbl]
        c.blacklist_cookie(r'domain "twitter\.com$"')
        c.add_cookie(cookies[0])
        self.other.send.assert_called_once_with(
            'cookie add ' + cookies[0])

    def test_filter_numeric(self):
        c = Cookies[self.uzbl]
        c.blacklist_cookie(r'0 "twitter\.com$"')
        c.add_cookie(cookies[1])
        self.uzbl.send.assert_called_once_with(
            'cookie delete ' + cookies[1])


class PrivateCookieTest(unittest.TestCase):
    def setUp(self):
        self.event_manager = EventManagerMock(
            (), (Cookies,),
            (), ((Config, dict),),
            config
        )
        self.priv = self.event_manager.add()
        self.uzbl_a = self.event_manager.add()
        self.uzbl_b = self.event_manager.add()

        Config[self.priv]['enable_private'] = 1

    def test_does_not_send_from_private_uzbl(self):
        c = Cookies[self.priv]
        c.add_cookie(cookies[0])

        self.uzbl_a.send.assert_not_called()
        self.uzbl_b.send.assert_not_called()

    def test_does_not_send_to_private_uzbl(self):
        c = Cookies[self.uzbl_a]
        c.add_cookie(cookies[0])
        self.priv.send.assert_not_called()


if __name__ == '__main__':
    unittest.main()
