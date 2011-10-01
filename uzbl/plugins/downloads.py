# this plugin does a very simple display of download progress. to use it, add
# @downloads to your status_format.

import os
from cgi import escape as htmlescape

from uzbl.arguments import splitquoted
from .config import Config
from uzbl.ext import PerInstancePlugin

class Downloads(PerInstancePlugin):

    def __init__(self, uzbl):
        super(Downloads, self).__init__(uzbl)
        uzbl.connect('DOWNLOAD_STARTED', self.download_started)
        uzbl.connect('DOWNLOAD_PROGRESS', self.download_progress)
        uzbl.connect('DOWNLOAD_COMPLETE', self.download_complete)
        self.active_downloads = {}

    def update_download_section(self):
        """after a download's status has changed this
           is called to update the status bar
        """

        if self.active_downloads:
            # add a newline before we list downloads
            result = '&#10;downloads:'
            for path, progress in self.active_downloads.items():
                # add each download
                fn = os.path.basename(path)

                dl = " %s (%d%%)" % (fn, progress * 100)

                # replace entities to make sure we don't break our markup
                # (this could be done with an @[]@ expansion in uzbl, but then we
                # can't use the &#10; above to make a new line)
                dl = htmlescape(dl)
                result += dl
        else:
            result = ''

        # and the result gets saved to an uzbl variable that can be used in
        # status_format
        config = Config[self.uzbl]
        if config.get('downloads', '') != result:
              config['downloads'] = result

    def download_started(self, args):
        # parse the arguments
        args = splitquoted(args)
        destination_path = args[0]

        # add to the list of active downloads
        self.active_downloads[destination_path] = 0.0

        # update the progress
        self.update_download_section()

    def download_progress(self, args):
        # parse the arguments
        args = splitquoted(args)
        destination_path = args[0]
        progress = float(args[1])

        # update the progress
        self.active_downloads[destination_path] = progress

        # update the status bar variable
        self.update_download_section()

    def download_complete(self, args):
        # TODO(tailhook) be more userfriendly: show download for some time!

        # parse the arguments
        args = splitquoted(args)
        destination_path = args[0]

        # remove from the list of active downloads
        del self.active_downloads[destination_path]

        # update the status bar variable
        self.update_download_section()

