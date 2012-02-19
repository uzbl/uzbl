from re import compile
from functools import partial

import uzbl.plugins.config
from .cmd_expand import cmd_expand
from uzbl.arguments import splitquoted
from uzbl.ext import PerInstancePlugin
import collections

valid_glob = compile('^[A-Za-z0-9_\*\.]+$').match

def make_matcher(glob):
    '''Make matcher function from simple glob.'''

    pattern = "^%s$" % glob.replace('*', '[^\s]*')
    return compile(pattern).match


class OnSetPlugin(PerInstancePlugin):

    def __init__(self, uzbl):
        super(OnSetPlugin, self).__init__(uzbl)
        self.on_sets = {}
        uzbl.connect('ON_SET', self.parse_on_set)
        uzbl.connect('CONFIG_CHANGED', self.check_for_handlers)

    def _exec_handlers(self, handlers, key, arg):
        '''Execute the on_set handlers that matched the key.'''

        for handler in handlers:
            if isinstance(handler, collections.Callable):
                handler(key, arg)
            else:
                self.uzbl.send(cmd_expand(handler, [key, arg]))

    def check_for_handlers(self, key, arg):
        '''Check for handlers for the current key.'''

        for (matcher, handlers) in list(self.on_sets.values()):
            if matcher(key):
                self._exec_handlers(handlers, key, arg)

    def on_set(self, glob, handler, prepend=True):
        '''Add a new handler for a config key change.

        Structure of the `self.on_sets` dict:
          { glob : ( glob matcher function, handlers list ), .. }
        '''

        assert valid_glob(glob)

        while '**' in glob:
            glob = glob.replace('**', '*')

        if isinstance(handler, collections.Callable):
            orig_handler = handler
            if prepend:
                handler = partial(handler, self.uzbl)

        else:
            orig_handler = handler = str(handler)

        if glob in self.on_sets:
            (matcher, handlers) = self.on_sets[glob]
            handlers.append(handler)

        else:
            matcher = make_matcher(glob)
            self.on_sets[glob] = (matcher, [handler,])

        self.logger.info('on set %r call %r' % (glob, orig_handler))


    def parse_on_set(self, args):
        '''Parse `ON_SET <glob> <command>` event then pass arguments to the
        `on_set(..)` function.'''

        args = splitquoted(args)
        assert len(args) >= 2
        glob = args[0]
        command = args.raw(1)

        assert glob and command and valid_glob(glob)
        self.on_set(glob, command)

