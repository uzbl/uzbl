#!/usr/bin/env python
# vi: set et ts=4:

import six
import unittest
from mock import Mock
from uzbl.core import Uzbl


class TestUzbl(unittest.TestCase):
    def setUp(self):
        options = Mock()
        options.print_events = False
        self.em = Mock()
        self.proto = Mock()
        self.uzbl = Uzbl(self.em, self.proto, options)

    def test_repr(self):
        r = '%r' % self.uzbl
        six.assertRegex(self, r, r'<uzbl\(.*\)>')

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

    def test_malformed_message(self):
        # Should not crash
        self.uzbl.parse_msg('asdaf')
        self.assertTrue(True)

    def test_send(self):
        self.uzbl.send('hello  ')
        self.proto.push.assert_called_once_with('hello\n'.encode('utf-8'))

    def test_close_calls_remove_instance(self):
        self.uzbl.close()
        self.em.remove_instance.assert_called_once_with(self.proto.socket)

    def test_close_cleans_plugins(self):
        p1, p2 = Mock(), Mock()
        self.uzbl._plugin_instances = (p1, p2)
        self.uzbl.plugins = {}
        self.uzbl.close()
        p1.cleanup.assert_called_once_with()
        p2.cleanup.assert_called_once_with()

    def test_close_connection_closes_protocol(self):
        self.uzbl.close_connection(Mock())
        self.proto.close.assert_called_once_with()

    def test_exit_triggers_close(self):
        self.uzbl.parse_msg(' '.join(['EVENT', 'spam', 'INSTANCE_EXIT']))
        self.em.remove_instance.assert_called_once_with(self.proto.socket)

    def test_instance_start(self):
        pid = 1234
        self.em.plugind.per_instance_plugins = []
        self.uzbl.parse_msg(
            ' '.join(['EVENT', 'spam', 'INSTANCE_START', str(pid)])
        )
        self.assertEqual(self.uzbl.pid, pid)

    def test_init_plugins(self):
        u = self.uzbl
        class FooPlugin(object):
            def __init__(self, uzbl): pass
        class BarPlugin(object):
            def __init__(self, uzbl): pass
        self.em.plugind.per_instance_plugins = [FooPlugin, BarPlugin]
        u.init_plugins()
        self.assertEqual(len(u.plugins), 2)
        for t in (FooPlugin, BarPlugin):
            self.assertIn(t, u.plugins)
            self.assertTrue(isinstance(u.plugins[t], t))
