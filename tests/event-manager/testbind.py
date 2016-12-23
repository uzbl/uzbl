#!/usr/bin/env python


import unittest
from emtest import EventManagerMock
from uzbl.plugins.bind import Bind, BindPlugin
from uzbl.plugins.config import Config


def justafunction():
    pass


class BindTest(unittest.TestCase):
    def test_unique_id(self):
        a = Bind('spam', 'spam')
        b = Bind('spam', 'spam')
        self.assertNotEqual(a.bid, b.bid)


class BindPluginTest(unittest.TestCase):
    def setUp(self):
        self.event_manager = EventManagerMock((), (Config, BindPlugin))
        self.uzbl = self.event_manager.add()

    def test_add_bind(self):
        b = BindPlugin[self.uzbl]
        modes = 'global'
        glob = 'test'
        handler = justafunction
        b.mode_bind(modes, glob, handler)

        binds = b.bindlet.get_binds()
        self.assertEqual(len(binds), 1)
        self.assertIs(binds[0].function, justafunction)

    def test_parse_bind(self):
        b = BindPlugin[self.uzbl]
        modes = 'global'
        glob = 'test'
        handler = 'handler'

        b.parse_mode_bind('%s %s = %s' % (modes, glob, handler))
        binds = b.bindlet.get_binds()
        self.assertEqual(len(binds), 1)
        self.assertEqual(binds[0].glob, glob)
        self.assertEqual(binds[0].commands, [handler])

    def test_parse_nasty_bind(self):
        b = BindPlugin[self.uzbl]
        modes = 'global'
        glob = '\'x'
        handler = 'do \'something\''

        b.parse_mode_bind('%s "%s" = %s' % (modes, glob, handler))
        binds = b.bindlet.get_binds()
        self.assertEqual(len(binds), 1)
        self.assertEqual(binds[0].glob, glob)
        self.assertEqual(binds[0].commands, [handler])
