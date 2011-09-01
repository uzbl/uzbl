#!/usr/bin/env python
import unittest
from doctest import DocTestSuite

keycmd_tests = DocTestSuite('uzbl.plugins.keycmd')
arguments_tests = DocTestSuite('uzbl.arguments')

tests = unittest.TestSuite()
tests.addTest(keycmd_tests)
tests.addTest(arguments_tests)

if __name__ == '__main__':
    runner = unittest.TextTestRunner()
    runner.run(tests)
