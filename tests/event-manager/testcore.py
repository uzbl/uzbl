#!/usr/bin/env python
# vi: set et ts=4:



import unittest
from mock import Mock
from uzbl.core import Uzbl


class TestUzbl(unittest.TestCase):
    def setUp(self):
        self.em = Mock()
        self.proto = Mock()
        self.uzbl = Uzbl(self.em, self.proto, Mock())

    def test_repr(self):
        r = '%r' % self.uzbl
        self.assertRegex(r, r'<uzbl\(.*\)>')

    def test_event_handler(self):
        handler = Mock()
        event, arg = 'FOO', 'test'
        self.uzbl.connect(event, handler)
        self.uzbl.event(event, arg)
        handler.assert_called_once_with(arg)

    def test_parse_sets_name(self):
        name = 'spam'
        self.uzbl.parse_msg(' '.join(['EVENT', name, 'FOO', 'BAR']))
        self.assertEqual(self.uzbl.name, name)

    def test_parse_sends_event(self):
        handler = Mock()
        event, arg = 'FOO', 'test'
        self.uzbl.connect(event, handler)
        self.uzbl.parse_msg(' '.join(['EVENT', 'instance-name', event, arg]))
        handler.assert_called_once_with(arg)

    def test_send(self):
        self.uzbl.send('hello  ')
        self.proto.push.assert_called_once_with('hello\n'.encode('utf-8'))

    def test_close_calls_remove_instance(self):
        self.uzbl.close()
        self.em.remove_instance.assert_called_once_with(self.proto.socket)

    def test_exit_triggers_close(self):
        self.uzbl.parse_msg(' '.join(['EVENT', 'spam', 'INSTANCE_EXIT']))
        self.em.remove_instance.assert_called_once_with(self.proto.socket)
