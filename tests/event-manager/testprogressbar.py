#!/usr/bin/env python
# vi: set et ts=4:



import sys
if '' not in sys.path:
    sys.path.insert(0, '')

import unittest
from emtest import EventManagerMock
from uzbl.plugins.config import Config
from uzbl.plugins.progress_bar import ProgressBar


class ProgressBarTest(unittest.TestCase):
    def setUp(self):
        self.event_manager = EventManagerMock(
            (), (ProgressBar,),
            (), ((Config, dict),)
        )
        self.uzbl = self.event_manager.add()

    def test_percent_done(self):
        uzbl = self.uzbl
        p, c = ProgressBar[uzbl], Config[uzbl]
        c['progress.format'] = '%c'

        p.update_progress()
        inout = (
            (9,   '9%'),
            (99,  '99%'),
            (100, '100%'),
            #(101, '100%') # TODO
        )

        for i, o in inout:
            p.update_progress(i)
            self.assertEqual(c['progress.output'], o)

    def test_done_char(self):
        uzbl = self.uzbl
        p, c = ProgressBar[uzbl], Config[uzbl]
        c['progress.format'] = '%d'

        p.update_progress()
        inout = (
            (9,   '='),
            (50,  '===='),
            (99,  '========'),
            (100, '========'),
            (101, '========')
        )

        for i, o in inout:
            p.update_progress(i)
            self.assertEqual(c['progress.output'], o)

    def test_pending_char(self):
        uzbl = self.uzbl
        p, c = ProgressBar[uzbl], Config[uzbl]
        c['progress.format'] = '%p'
        c['progress.pending'] = '-'

        p.update_progress()
        inout = (
            (9,   '-------'),
            (50,  '----'),
            (99,  ''),
            (100, ''),
            (101, '')
        )

        for i, o in inout:
            p.update_progress(i)
            self.assertEqual(c['progress.output'], o)

    def test_percent_pending(self):
        uzbl = self.uzbl
        p, c = ProgressBar[uzbl], Config[uzbl]
        c['progress.format'] = '%t'

        p.update_progress()
        inout = (
            (9,   '91%'),
            (50,  '50%'),
            (99,  '1%'),
            (100, '0%'),
            #(101, '0%')  # TODO
        )

        for i, o in inout:
            p.update_progress(i)
            self.assertEqual(c['progress.output'], o)
