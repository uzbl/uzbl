# vi: set et ts=4:



import unittest
from emtest import EventManagerMock

from uzbl.plugins.config import Config
from uzbl.plugins.downloads import Downloads


class DownloadsTest(unittest.TestCase):
    def setUp(self):
        self.event_manager = EventManagerMock((), (Downloads, Config))
        self.uzbl = self.event_manager.add()

    def test_start(self):
        cases = (
            ('foo', 'foo', 'foo (0%)'),
            ('"b@r"', 'b@r', 'b@r (0%'),
        )
        d = Downloads[self.uzbl]
        for input, key, section in cases:
            d.download_started(input)
            self.assertIn(key, d.active_downloads)
            self.assertEqual(d.active_downloads[key], 0)
            self.uzbl.send.assert_called_once()
            self.assertIn(section, self.uzbl.send.call_args[0][0])
            self.uzbl.reset_mock()
