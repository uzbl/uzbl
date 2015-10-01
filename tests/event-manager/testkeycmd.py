#!/usr/bin/env python

import re
import mock
import unittest
from emtest import EventManagerMock
from uzbl.plugins.keycmd import KeyCmd
from uzbl.plugins.config import Config


def getkeycmd(s):
    return re.match(r'@\[([^\]]*)\]@', s).group(1)


class KeyCmdTest(unittest.TestCase):
    def setUp(self):
        self.event_manager = EventManagerMock(
            (), (KeyCmd,),
            (), ((Config, dict),)
        )
        self.uzbl = self.event_manager.add()

    def test_press_key(self):
        c, k = Config[self.uzbl], KeyCmd[self.uzbl]
        k.key_press(('', 'a'))
        self.assertEqual(c.get('modcmd', ''), '')
        keycmd = getkeycmd(c['keycmd'])
        self.assertEqual(keycmd, 'a')

    def test_press_keys(self):
        c, k = Config[self.uzbl], KeyCmd[self.uzbl]
        string = 'uzbl'
        for char in string:
            k.key_press(('', char))
        self.assertEqual(c.get('modcmd', ''), '')
        keycmd = getkeycmd(c['keycmd'])
        self.assertEqual(keycmd, string)

    def test_press_unicode_keys(self):
        c, k = Config[self.uzbl], KeyCmd[self.uzbl]
        string = u'\u5927\u962a\u5e02'
        for char in string:
            k.key_press(('', char))
        self.assertEqual(c.get('modcmd', ''), '')
        keycmd = getkeycmd(c['keycmd'])
        self.assertEqual(keycmd, string)
