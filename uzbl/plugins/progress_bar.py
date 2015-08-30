import re
from .config import Config

from uzbl.ext import PerInstancePlugin

class ProgressBar(PerInstancePlugin):
    splitfrmt = re.compile(r'(%[A-Z][^%]|%[^%])').split

    def __init__(self, uzbl):
        super(ProgressBar, self).__init__(uzbl)
        uzbl.connect('LOAD_COMMIT', lambda uri: self.update_progress())
        uzbl.connect('LOAD_PROGRESS', self.update_progress)
        self.updates = 0

    def update_progress(self, progress=None):
        '''Updates the progress.output variable on LOAD_PROGRESS update.

        The current substitution options are:
            %d = done char * done
            %p = pending char * remaining
            %c = percent done
            %i = int done
            %s = -\|/ spinner
            %t = percent pending
            %o = int pending
            %r = sprites

        Default configuration options:
            progress.format  = [%d>%p]%c
            progress.width   = 8
            progress.done    = =
            progress.pending =
            progress.spinner = -\|/
            progress.sprites = loading
        '''

        if progress is None:
            self.updates = 0
            progress = 100

        else:
            self.updates += 1
            progress = int(progress)

        # Get progress config vars.
        config = Config[self.uzbl]
        frmt = config.get('progress.format', '[%d>%p]%c')
        width = int(config.get('progress.width', 8))
        done_symbol = config.get('progress.done', '=')
        pend = config.get('progress.pending', None)
        pending_symbol = pend if pend else ' '

        # Get spinner character
        spinner = config.get('progress.spinner', '-\\|/')
        index = 0 if progress == 100 else self.updates % len(spinner)
        spinner = '\\\\' if spinner[index] == '\\' else spinner[index]

        # get sprite character
        sprites = config.get('progress.sprites', 'loading')
        index = int(((progress/100.0)*len(sprites))+0.5)-1
        sprite = '%r' % ('\\\\' if sprites[index] == '\\' else sprites[index])

        # Inflate the done and pending bars to stop the progress bar
        # jumping around.
        if '%c' in frmt or '%i' in frmt:
            count = frmt.count('%c') + frmt.count('%i')
            width += (3-len(str(progress))) * count

        if '%t' in frmt or '%o' in frmt:
            count = frmt.count('%t') + frmt.count('%o')
            width += (3-len(str(100-progress))) * count

        done = int(((progress/100.0)*width)+0.5)
        pending = width - done

        # values to replace with
        values = {
            'd': (done_symbol * done)[:done],
            'p': (pending_symbol * pending)[:pending],
            'c': '%d%%' % progress,
            'i': '%d' % progress,
            't': '%d%%' % (100 - progress),
            'o': '%d' % (100 - progress),
            's': spinner,
            'r': sprite,
            '%': '%'
        }

        frmt = ''.join([str(values[k[1:]]) if k.startswith('%') else k for
                k in self.splitfrmt(frmt)])

        if config.get('progress.output', None) != frmt:
            config['progress.output'] = frmt
