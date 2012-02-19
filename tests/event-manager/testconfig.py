

import unittest
from emtest import EventManagerMock

from uzbl.plugins.config import Config


class ConfigTest(unittest.TestCase):
    def setUp(self):
        self.event_manager = EventManagerMock((), (Config,))
        self.uzbl = self.event_manager.add()

    def test_set(self):
        cases = (
            (True, '1'),
            (False, '0'),
            ("test", "test"),
            (5, '5')
        )
        c = Config[self.uzbl]
        for input, expected in cases:
            c.set('foo', input)
            self.uzbl.send.assert_called_once_with(
                'set foo = ' + expected)
            self.uzbl.send.reset_mock()

    def test_set_invalid(self):
        cases = (
            ("foo\nbar", AssertionError),  # Better Exception type
            ("bad'key", AssertionError)
        )
        c = Config[self.uzbl]
        for input, exception in cases:
            self.assertRaises(exception, c.set, input)

    def test_parse(self):
        cases = (
            ('foo str value', 'foo', 'value'),
            ('foo str "ba ba"', 'foo', 'ba ba'),
            ('foo float 5', 'foo', 5.0)
        )
        c = Config[self.uzbl]
        for input, ekey, evalue in cases:
            c.parse_set_event(input)
            self.assertIn(ekey, c)
            self.assertEqual(c[ekey], evalue)
            self.uzbl.event.assert_called_once_with(
                'CONFIG_CHANGED', ekey, evalue)
            self.uzbl.event.reset_mock()

    def test_parse_null(self):
        cases = (
            ('foo str', 'foo'),
            ('foo str ""', 'foo'),
            #('foo int', 'foo')  # Not sure if this input is valid
        )
        c = Config[self.uzbl]
        for input, ekey in cases:
            c.update({'foo': '-'})
            c.parse_set_event(input)
            self.assertNotIn(ekey, c)
            self.uzbl.event.assert_called_once_with(
                'CONFIG_CHANGED', ekey, '')
            self.uzbl.event.reset_mock()

    def test_parse_invalid(self):
        cases = (
            ('foo bar', AssertionError),  # TypeError?
            ('foo bad^key', AssertionError),
            ('', Exception),
            ('foo int z', ValueError)
        )
        c = Config[self.uzbl]
        for input, exception in cases:
            self.assertRaises(exception, c.parse_set_event, input)
            self.assertEqual(len(list(c.keys())), 0)
