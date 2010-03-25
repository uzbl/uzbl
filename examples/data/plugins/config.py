import re
import types

__export__ = ['set', 'get_config']

VALIDKEY = re.compile("^[a-zA-Z][a-zA-Z0-9_]*$").match
TYPECONVERT = {'int': int, 'float': float, 'str': unicode}

UZBLS = {}


def escape(value):
    '''A real escaping function may be required.'''

    return unicode(value)


def set(uzbl, key, value='', config=None, force=False):
    '''Sends a: "set key = value" command to the uzbl instance. If force is
    False then only send a set command if the values aren't equal.'''

    if type(value) == types.BooleanType:
        value = int(value)

    else:
        value = unicode(value)

    if not VALIDKEY(key):
        raise KeyError("%r" % key)

    value = escape(value)
    if '\n' in value:
        value = value.replace("\n", "\\n")

    if not force:
        if config is None:
            config = get_config(uzbl)

        if key in config and config[key] == value:
            return

    uzbl.send('set %s = %s' % (key, value))


class ConfigDict(dict):
    def __init__(self, uzbl):
        self._uzbl = uzbl

    def __setitem__(self, key, value):
        '''Makes "config[key] = value" a wrapper for the set function.'''

        set(self._uzbl, key, value, config=self)


def add_instance(uzbl, *args):
    UZBLS[uzbl] = ConfigDict(uzbl)


def del_instance(uzbl, *args):
    if uzbl in UZBLS:
        del uzbl


def get_config(uzbl):
    if uzbl not in UZBLS:
        add_instance(uzbl)

    return UZBLS[uzbl]


def variable_set(uzbl, args):
    config = get_config(uzbl)

    key, type, value = list(args.split(' ', 2) + ['',])[:3]
    old = config[key] if key in config else None
    value = TYPECONVERT[type](value)

    dict.__setitem__(config, key, value)

    if old != value:
        uzbl.event("CONFIG_CHANGED", key, value)


def init(uzbl):
    # Event handling hooks.
    uzbl.connect_dict({
        'INSTANCE_EXIT':    del_instance,
        'INSTANCE_START':   add_instance,
        'VARIABLE_SET':     variable_set,
    })

    # Function exports to the uzbl object, `function(uzbl, *args, ..)`
    # becomes `uzbl.function(*args, ..)`.
    uzbl.export_dict({
        'get_config':   get_config,
        'set':          set,
    })
