#!/usr/bin/env python
# vi: set et ts=4:

from __future__ import print_function

import sys
if '' not in sys.path:
    sys.path.insert(0, '')

import unittest
from emtest import EventManagerMock

from uzbl.plugins.history import History, SharedHistory
from uzbl.plugins.keycmd import Keylet, KeyCmd
from uzbl.plugins.on_set import OnSetPlugin
from uzbl.plugins.config import Config


class SharedHistoryTest(unittest.TestCase):
    def setUp(self):
        self.event_manager = EventManagerMock((SharedHistory,), ())
        self.uzbl = self.event_manager.add()
        self.other = self.event_manager.add()

    def test_instance(self):
        a = SharedHistory[self.uzbl]
        b = SharedHistory[self.other]
        self.assertIs(a, b)

    def test_add_and_get(self):
        s = SharedHistory[self.uzbl]
        s.addline('foo', 'bar')
        s.addline('foo', 'baz')
        s.addline('foo', 'bap')
        self.assertEquals(s.get_line_number('foo'), 3)
        self.assertEquals(s.get_line_number('other'), 0)
        self.assertEquals(s.getline('foo', 0), 'bar')
        self.assertEquals(s.getline('foo', 1), 'baz')
        self.assertEquals(s.getline('foo', 2), 'bap')
        self.assertEquals(s.getline('foo', -1), 'bap')

    def test_empty_line_number(self):
        s = SharedHistory[self.uzbl]
        s.addline('foo', 'bar')
        self.assertEquals(s.get_line_number(''), 0)
        self.assertEquals(s.get_line_number('other'), 0)

    def test_get_missing_prompt(self):
        s = SharedHistory[self.uzbl]
        s.addline('foo', 'bar')
        self.assertRaises(IndexError, s.getline, 'bar', 0)


class HistoryTest(unittest.TestCase):
    def setUp(self):
        self.event_manager = EventManagerMock(
            (SharedHistory,),
            (OnSetPlugin, KeyCmd, Config, History)
        )
        self.uzbl = self.event_manager.add()
        self.other = self.event_manager.add()
        s = SharedHistory[self.uzbl]
        data = (
            ('', 'woop'),
            ('', 'doop'),
            ('', 'bar'),
            ('', 'foo'),
            ('git', 'spam'),
            ('git', 'egg'),
            ('foo', 'foo')
        )
        for prompt, input in data:
            s.addline(prompt, input)

    def test_step(self):
        h = History[self.uzbl]
        self.assertEquals('', h.next())
        self.assertEquals('', h.next())
        self.assertEquals('foo', h.prev())
        self.assertEquals('bar', h.prev())
        self.assertEquals('foo', h.next())
        self.assertEquals('bar', h.prev())
        self.assertEquals('doop', h.prev())
        self.assertEquals('woop', h.prev())
        self.assertTrue(len(h.prev()) > 0)
        self.assertTrue(len(h.prev()) > 0)
        self.assertEquals('woop', h.next())

    def test_step_prompt(self):
        h = History[self.uzbl]
        h.change_prompt('git')
        self.assertEquals('', h.next())
        self.assertEquals('', h.next())
        self.assertEquals('egg', h.prev())
        self.assertEquals('spam', h.prev())
        self.assertTrue(len(h.prev()) > 0)
        self.assertTrue(len(h.prev()) > 0)
        self.assertEquals('spam', h.next())

    def test_change_prompt(self):
        h = History[self.uzbl]
        self.assertEquals('foo', h.prev())
        self.assertEquals('bar', h.prev())
        h.change_prompt('git')
        self.assertEquals('egg', h.prev())
        self.assertEquals('spam', h.prev())

    def test_exec(self):
        modstate = set()
        keylet = Keylet()
        keylet.set_keycmd('foo')
        History[self.uzbl].keycmd_exec(modstate, keylet)
        s = SharedHistory[self.uzbl]
        self.assertEqual(s.getline('', -1), 'foo')

    def test_exec_from_history(self):
        h = History[self.uzbl]
        self.assertEquals('foo', h.prev())
        self.assertEquals('bar', h.prev())
        self.assertEquals('doop', h.prev())
        modstate = set()
        keylet = Keylet()
        keylet.set_keycmd('doop')
        h.keycmd_exec(modstate, keylet)
        self.assertEquals('doop', h.prev())
        self.assertEquals('foo', h.prev())
        self.assertEquals('bar', h.prev())
        # do we really want this one here ?
        self.assertEquals('doop', h.prev())
        self.assertEquals('woop', h.prev())

    def test_search(self):
        h = History[self.uzbl]
        h.search('oop')
        self.assertEquals('doop', h.prev())
        self.assertEquals('woop', h.prev())
        self.assertTrue(len(h.prev()) > 0)
        self.assertEquals('woop', h.next())
        self.assertEquals('doop', h.next())
        # this reset the search
        self.assertEquals('', h.next())
        self.assertEquals('foo', h.prev())

    def test_temp(self):
        kl = KeyCmd[self.uzbl].keylet
        kl.set_keycmd('uzbl')
        h = History[self.uzbl]
        h.change_prompt('foo')
        # Why is the preserve current logic in this method?
        h.history_prev(None)
        self.assertTrue(len(h.prev()) > 0)
        self.assertEquals('foo', h.next())
        self.assertEquals('uzbl', h.next())
        self.assertEquals('', h.next())  # this clears the keycmd
