#!/usr/bin/env python

import sys
if '' not in sys.path:
	sys.path.insert(0, '')

import unittest
from doctest import DocTestSuite
keycmd_tests = DocTestSuite('uzbl.plugins.keycmd')
arguments_tests = DocTestSuite('uzbl.arguments')

def load_tests(loader, standard, pattern):
	tests = unittest.TestSuite()
	tests.addTest(keycmd_tests)
	tests.addTest(arguments_tests)
	return tests

if __name__ == '__main__':
    runner = unittest.TextTestRunner()
    runner.run()
