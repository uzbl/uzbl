#!/usr/bin/env python
# vi: set et ts=4:



import unittest
from mock import Mock
from uzbl.core import Uzbl


class TestUzbl(unittest.TestCase):
    def setUp(self):
        self.em = Mock()
        self.uzbl = Uzbl(self.em, Mock(), Mock())

    def test_repr(self):
        r = '%r' % self.uzbl
        self.assertRegex(r, r'<uzbl\(.*\)>')

    def test_event_handler(self):
        handler = Mock()
        arg = 'test'
        self.uzbl.connect('FOO', handler)
        self.uzbl.event('FOO', arg)
        handler.assert_called_once_with(arg)
