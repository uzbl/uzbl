#!/usr/bin/env python

import unittest
from uzbl.arguments import Arguments


class ArgumentsTest(unittest.TestCase):
    def test_empty(self):
        a = Arguments('')
        self.assertEqual(len(a), 0)
        self.assertEqual(a.raw(), '')

    def test_space(self):
        a = Arguments(' foo  bar')
        self.assertEqual(len(a), 2)
        self.assertEqual(a.raw(), 'foo  bar')
        self.assertEqual(a.raw(0, 0), 'foo')
        self.assertEqual(a.raw(1, 1), 'bar')

    def test_tab(self):
        a = Arguments('\tfoo\t\tbar')
        self.assertEqual(len(a), 2)
        self.assertEqual(a.raw(), 'foo\t\tbar')
        self.assertEqual(a.raw(0, 0), 'foo')
        self.assertEqual(a.raw(1, 1), 'bar')

    def test_quoted_quote(self):
        a = Arguments('foo "\'"')
        self.assertEqual(a, ('foo', "'"))

    def test_expand(self):
        a = Arguments('foo @<some uzbl.command("foo")>@')
        self.assertEquals(a, ('foo', '@<some uzbl.command("foo")>@'))

    def test_expand_sub_var(self):
        a = Arguments('foo @<some uzbl.command( "@var", "bla" )>@')
        self.assertEquals(a, ('foo', '@<some uzbl.command( "@var", "bla" )>@'))

    def test_escape(self):
        a = Arguments('foo "\\\\" asd\ af')
        self.assertEquals(a, ('foo', '\\', 'asd af'))
