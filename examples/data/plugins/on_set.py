from re import compile
from functools import partial

valid_glob = compile('^[A-Za-z0-9_\*\.]+$').match

def make_matcher(glob):
    '''Make matcher function from simple glob.'''

    pattern = "^%s$" % glob.replace('*', '[^\s]*')
    return compile(pattern).match


def exec_handlers(uzbl, handlers, key, arg):
    '''Execute the on_set handlers that matched the key.'''

    for handler in handlers:
        if callable(handler):
            handler(key, arg)

        else:
            uzbl.send(uzbl.cmd_expand(handler, [key, arg]))


def check_for_handlers(uzbl, key, arg):
    '''Check for handlers for the current key.'''

    for (matcher, handlers) in uzbl.on_sets.values():
        if matcher(key):
            exec_handlers(uzbl, handlers, key, arg)


def on_set(uzbl, glob, handler, prepend=True):
    '''Add a new handler for a config key change.

    Structure of the `uzbl.on_sets` dict:
      { glob : ( glob matcher function, handlers list ), .. }
    '''

    assert valid_glob(glob)

    while '**' in glob:
        glob = glob.replace('**', '*')

    if callable(handler):
        orig_handler = handler
        if prepend:
            handler = partial(handler, uzbl)

    else:
        orig_handler = handler = unicode(handler)

    if glob in uzbl.on_sets:
        (matcher, handlers) = uzbl.on_sets[glob]
        handlers.append(handler)

    else:
        matcher = make_matcher(glob)
        uzbl.on_sets[glob] = (matcher, [handler,])

    uzbl.logger.info('on set %r call %r' % (glob, orig_handler))


def parse_on_set(uzbl, args):
    '''Parse `ON_SET <glob> <command>` event then pass arguments to the
    `on_set(..)` function.'''

    (glob, command) = (args.split(' ', 1) + [None,])[:2]
    assert glob and command and valid_glob(glob)
    on_set(uzbl, glob, command)


# plugins init hook
def init(uzbl):
    require('config')
    require('cmd_expand')

    export_dict(uzbl, {
        'on_sets':  {},
        'on_set':   on_set,
    })

    connect_dict(uzbl, {
        'ON_SET':           parse_on_set,
        'CONFIG_CHANGED':   check_for_handlers,
    })

# plugins cleanup hook
def cleanup(uzbl):
    for (matcher, handlers) in uzbl.on_sets.values():
        del handlers[:]

    uzbl.on_sets.clear()
