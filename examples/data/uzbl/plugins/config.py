import re
import types

__export__ = ['set', 'get_config']

_VALIDSETKEY = re.compile("^[a-zA-Z][a-zA-Z0-9_]*$").match
_TYPECONVERT = {'int': int, 'float': float, 'str': unicode}

UZBLS = {}


def escape(value):
    '''A real escaping function may be required.'''

    return unicode(value)


def get_config(uzbl):
    if uzbl not in UZBLS:
        add_instance(uzbl)

    return UZBLS[uzbl]


def set(uzbl, key, value='', force=True):
    '''Sends a: "set key = value" command to the uzbl instance. If force is
    False then only send a set command if the values aren't equal.'''

    if type(value) == types.BooleanType:
        value = int(value)

    else:
        value = unicode(value)

    if not _VALIDSETKEY(key):
        raise KeyError("%r" % key)

    value = escape(value)
    if '\n' in value:
        value = value.replace("\n", "\\n")

    if not force:
        config = get_config(uzbl)
        if key in config and config[key] == value:
            return

    uzbl.send('set %s = %s' % (key, value))


def add_instance(uzbl, *args):
    UZBLS[uzbl] = ConfigDict(uzbl)


def del_instance(uzbl, *args):
    if uzbl in UZBLS:
        del uzbl


def get_config(uzbl):
    if uzbl not in UZBLS:
        add_instance(uzbl)

    return UZBLS[uzbl]


class ConfigDict(dict):
    def __init__(self, uzbl):
        self._uzbl = uzbl

    def __setitem__(self, key, value):
        '''Makes "config[key] = value" a wrapper for the set function.'''

        set(self._uzbl, key, value, force=False)


def variable_set(uzbl, args):
    config = get_config(uzbl)

    key, type, value = list(args.split(' ', 2) + ['',])[:3]
    old = config[key] if key in config else None
    value = _TYPECONVERT[type](value)

    dict.__setitem__(config, key, value)

    if old != value:
        uzbl.event("CONFIG_CHANGED", key, value)


def init(uzbl):

    connects = {'VARIABLE_SET': variable_set,
      'INSTANCE_START': add_instance,
      'INSTANCE_EXIT': del_instance}

    uzbl.connect_dict(connects)
