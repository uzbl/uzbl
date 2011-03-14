# this plugin does a very simple display of download progress. to use it, add
# @downloads to your status_format.

import os
ACTIVE_DOWNLOADS = {}

# after a download's status has changed this is called to update the status bar
def update_download_section(uzbl):
    global ACTIVE_DOWNLOADS

    if len(ACTIVE_DOWNLOADS):
        # add a newline before we list downloads
        result = '&#10;downloads:'
        for path in ACTIVE_DOWNLOADS:
            # add each download
            fn = os.path.basename(path)
            progress, = ACTIVE_DOWNLOADS[path]

            dl = " %s (%d%%)" % (fn, progress * 100)

            # replace entities to make sure we don't break our markup
            # (this could be done with an @[]@ expansion in uzbl, but then we
            # can't use the &#10; above to make a new line)
            dl = dl.replace("&", "&amp;").replace("<", "&lt;")
            result += dl
    else:
        result = ''

    # and the result gets saved to an uzbl variable that can be used in
    # status_format
    if uzbl.config.get('downloads', '') != result:
          uzbl.config['downloads'] = result

def download_started(uzbl, args):
    # parse the arguments
    args = splitquoted(args)
    destination_path = args[0]

    # add to the list of active downloads
    global ACTIVE_DOWNLOADS
    ACTIVE_DOWNLOADS[destination_path] = (0.0,)

    # update the progress
    update_download_section(uzbl)

def download_progress(uzbl, args):
    # parse the arguments
    args = splitquoted(args)
    destination_path = args[0]
    progress = float(args[1])

    # update the progress
    global ACTIVE_DOWNLOADS
    ACTIVE_DOWNLOADS[destination_path] = (progress,)

    # update the status bar variable
    update_download_section(uzbl)

def download_complete(uzbl, args):
    # parse the arguments
    args = splitquoted(args)
    destination_path = args[0]

    # remove from the list of active downloads
    global ACTIVE_DOWNLOADS
    del ACTIVE_DOWNLOADS[destination_path]

    # update the status bar variable
    update_download_section(uzbl)

# plugin init hook
def init(uzbl):
    connect_dict(uzbl, {
        'DOWNLOAD_STARTED':     download_started,
        'DOWNLOAD_PROGRESS':    download_progress,
        'DOWNLOAD_COMPLETE':    download_complete,
    })
