#!/usr/bin/env python


import sys
if '' not in sys.path:
    sys.path.insert(0, '')

import mock
import unittest
from emtest import EventManagerMock
from uzbl.plugins.bind import BindPlugin
from uzbl.plugins.config import Config


def justafunction():
    pass


class BindTest(unittest.TestCase):
    def setUp(self):
        self.event_manager = EventManagerMock((), (Config, BindPlugin))
        self.uzbl = self.event_manager.add()

    def test_add_bind(self):
        modes = 'global'
        glob = 'test'
        handler = justafunction
        BindPlugin[self.uzbl].mode_bind(modes, glob, handler)

        b = BindPlugin[self.uzbl].bindlet
        binds = b.get_binds()
        self.assertEqual(len(binds), 1)
        self.assertIs(binds[0].function, justafunction)
