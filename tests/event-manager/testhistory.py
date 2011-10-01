#!/usr/bin/env python
from __future__ import print_function

import sys
if '' not in sys.path:
	sys.path.insert(0, '')

import unittest
from emtest import EventManagerMock

from uzbl.plugins.history import History, SharedHistory
from uzbl.plugins.keycmd import Keylet
from uzbl.plugins.on_set import OnSetPlugin

class HistoryTest(unittest.TestCase):
	def setUp(self):
		self.event_manager = EventManagerMock((SharedHistory,), (OnSetPlugin, History))
		self.uzbl = self.event_manager.add()
		self.other = self.event_manager.add()

	def test_shared_history(self):
		a = SharedHistory[self.uzbl]
		b = SharedHistory[self.other]
		self.assertIs(a, b)

	def test_exec(self):
		modstate = set()
		keylet = Keylet()
		keylet.set_keycmd('foo')
		History[self.uzbl].keycmd_exec(modstate, keylet)
		
		self.assertEqual(SharedHistory[self.uzbl].getline('', 0), 'foo')
