#!/usr/bin/env python
# vi: set et ts=4:



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
        self.assertEqual(s.get_line_number('foo'), 3)
        self.assertEqual(s.get_line_number('other'), 0)
        self.assertEqual(s.getline('foo', 0), 'bar')
        self.assertEqual(s.getline('foo', 1), 'baz')
        self.assertEqual(s.getline('foo', 2), 'bap')
        self.assertEqual(s.getline('foo', -1), 'bap')

    def test_empty_line_number(self):
        s = SharedHistory[self.uzbl]
        s.addline('foo', 'bar')
        self.assertEqual(s.get_line_number(''), 0)
        self.assertEqual(s.get_line_number('other'), 0)

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
        self.assertEqual('', next(h))
        self.assertEqual('', next(h))
        self.assertEqual('foo', h.prev())
        self.assertEqual('bar', h.prev())
        self.assertEqual('foo', next(h))
        self.assertEqual('bar', h.prev())
        self.assertEqual('doop', h.prev())
        self.assertEqual('woop', h.prev())
        self.assertTrue(len(h.prev()) > 0)
        self.assertTrue(len(h.prev()) > 0)
        self.assertEqual('woop', next(h))

    def test_step_prompt(self):
        h = History[self.uzbl]
        h.change_prompt('git')
        self.assertEqual('', next(h))
        self.assertEqual('', next(h))
        self.assertEqual('egg', h.prev())
        self.assertEqual('spam', h.prev())
        self.assertTrue(len(h.prev()) > 0)
        self.assertTrue(len(h.prev()) > 0)
        self.assertEqual('spam', next(h))

    def test_change_prompt(self):
        h = History[self.uzbl]
        self.assertEqual('foo', h.prev())
        self.assertEqual('bar', h.prev())
        h.change_prompt('git')
        self.assertEqual('egg', h.prev())
        self.assertEqual('spam', h.prev())

    def test_exec(self):
        modstate = set()
        keylet = Keylet()
        keylet.set_keycmd('foo')
        History[self.uzbl].keycmd_exec(modstate, keylet)
        s = SharedHistory[self.uzbl]
        self.assertEqual(s.getline('', -1), 'foo')

    def test_exec_from_history(self):
        h = History[self.uzbl]
        self.assertEqual('foo', h.prev())
        self.assertEqual('bar', h.prev())
        self.assertEqual('doop', h.prev())
        modstate = set()
        keylet = Keylet()
        keylet.set_keycmd('doop')
        h.keycmd_exec(modstate, keylet)
        self.assertEqual('doop', h.prev())
        self.assertEqual('foo', h.prev())
        self.assertEqual('bar', h.prev())
        # do we really want this one here ?
        self.assertEqual('doop', h.prev())
        self.assertEqual('woop', h.prev())

    def test_search(self):
        h = History[self.uzbl]
        h.search('oop')
        self.assertEqual('doop', h.prev())
        self.assertEqual('woop', h.prev())
        self.assertTrue(len(h.prev()) > 0)
        self.assertEqual('woop', next(h))
        self.assertEqual('doop', next(h))
        # this reset the search
        self.assertEqual('', next(h))
        self.assertEqual('foo', h.prev())

    def test_temp(self):
        kl = KeyCmd[self.uzbl].keylet
        kl.set_keycmd('uzbl')
        h = History[self.uzbl]
        h.change_prompt('foo')
        # Why is the preserve current logic in this method?
        h.history_prev(None)
        self.assertTrue(len(h.prev()) > 0)
        self.assertEqual('foo', next(h))
        self.assertEqual('uzbl', next(h))
        self.assertEqual('', next(h))  # this clears the keycmd
