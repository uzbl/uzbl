#!/usr/bin/env python
from __future__ import print_function

import sys
if '' not in sys.path:
	sys.path.insert(0, '')

import unittest
from emtest import EventManagerMock

from uzbl.plugins.cookies import Cookies

cookies = (
	r'".nyan.cat" "/" "__utmb" "183192761.1.10.1313990640" "http" "1313992440"',
	r'".twitter.com" "/" "guest_id" "v1%3A131399064036991891" "http" "1377104460"'
)

class CookieFilterTest(unittest.TestCase):
	def setUp(self):
		self.event_manager = EventManagerMock((), (Cookies,))
		self.uzbl = self.event_manager.add()
		self.other = self.event_manager.add()

	def test_add_cookie(self):
		c = Cookies[self.uzbl]
		c.add_cookie(cookies[0])
		self.other.send.assert_called_once_with(
			'add_cookie ' + cookies[0])

	def test_whitelist_block(self):
		c = Cookies[self.uzbl]
		c.whitelist_cookie(r'domain "nyan\.cat$"')
		c.add_cookie(cookies[1])
		self.uzbl.send.assert_called_once_with(
			'delete_cookie ' + cookies[1])

	def test_whitelist_accept(self):
		c = Cookies[self.uzbl]
		c.whitelist_cookie(r'domain "nyan\.cat$"')
		c.add_cookie(cookies[0])
		self.other.send.assert_called_once_with(
			'add_cookie ' + cookies[0])

	def test_blacklist_block(self):
		c = Cookies[self.uzbl]
		c.blacklist_cookie(r'domain "twitter\.com$"')
		c.add_cookie(cookies[1])
		self.uzbl.send.assert_called_once_with(
			'delete_cookie ' + cookies[1])

	def test_blacklist_accept(self):
		c = Cookies[self.uzbl]
		c.blacklist_cookie(r'domain "twitter\.com$"')
		c.add_cookie(cookies[0])
		self.other.send.assert_called_once_with(
			'add_cookie ' + cookies[0])

	def test_filter_numeric(self):
		c = Cookies[self.uzbl]
		c.blacklist_cookie(r'0 "twitter\.com$"')
		c.add_cookie(cookies[1])
		self.uzbl.send.assert_called_once_with(
			'delete_cookie ' + cookies[1])

if __name__ == '__main__':
	unittest.main()
