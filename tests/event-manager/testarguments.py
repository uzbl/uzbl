#!/usr/bin/env python

import unittest
from uzbl.arguments import Arguments

class ArgumentsTest(unittest.TestCase):
	def test_empty(self):
		a = Arguments('')
		self.assertEquals(len(a), 0)
		self.assertEquals(a.raw(), '')

	def test_space(self):
		a = Arguments(' foo  bar')
		self.assertEquals(len(a), 2)
		self.assertEquals(a.raw(), 'foo  bar')
		self.assertEquals(a.raw(0, 0), 'foo')
		self.assertEquals(a.raw(1, 1), 'bar')

	def test_tab(self):
		a = Arguments('\tfoo\t\tbar')
		self.assertEquals(len(a), 2)
		self.assertEquals(a.raw(), 'foo\t\tbar')
		self.assertEquals(a.raw(0, 0), 'foo')
		self.assertEquals(a.raw(1, 1), 'bar')
