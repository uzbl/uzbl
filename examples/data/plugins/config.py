from re import compile
from types import BooleanType
from UserDict import DictMixin

valid_key = compile('^[A-Za-z0-9_\.]+$').match

class Config(DictMixin):
    def __init__(self, uzbl):
        self.uzbl = uzbl

        # Create the base dict and map allowed methods to `self`.
        self.data = data = {}

        methods = ['__contains__', '__getitem__', '__iter__',
            '__len__',  'get', 'has_key', 'items', 'iteritems',
            'iterkeys', 'itervalues', 'values']

        for method in methods:
            setattr(self, method, getattr(data, method))


    def __setitem__(self, key, value):
        self.set(key, value)

    def __delitem__(self, key):
        self.set(key)

    def update(self, other=None, **kwargs):
        if other is None:
            other = {}

        for (key, value) in dict(other).items() + kwargs.items():
            self[key] = value


    def set(self, key, value='', force=False):
        '''Generates a `set <key> = <value>` command string to send to the
        current uzbl instance.

        Note that the config dict isn't updated by this function. The config
        dict is only updated after a successful `VARIABLE_SET ..` event
        returns from the uzbl instance.'''

        assert valid_key(key)

        if type(value) == BooleanType:
            value = int(value)

        else:
            value = unicode(value)
            assert '\n' not in value

        if not force and key in self and self[key] == value:
            return

        self.uzbl.send(u'set %s = %s' % (key, value))


def parse_set_event(uzbl, args):
    '''Parse `VARIABLE_SET <var> <type> <value>` event and load the
    (key, value) pair into the `uzbl.config` dict.'''

    (key, type, raw_value) = (args.split(' ', 2) + ['',])[:3]

    assert valid_key(key)
    assert type in types

    new_value = types[type](raw_value)
    old_value = uzbl.config.get(key, None)

    # Update new value.
    uzbl.config.data[key] = new_value

    if old_value != new_value:
        uzbl.event('CONFIG_CHANGED', key, new_value)

    # Cleanup null config values.
    if type == 'str' and not new_value:
        del uzbl.config.data[key]


# plugin init hook
def init(uzbl):
    global types
    types = {'int': int, 'float': float, 'str': unquote}
    export(uzbl, 'config', Config(uzbl))
    connect(uzbl, 'VARIABLE_SET', parse_set_event)

# plugin cleanup hook
def cleanup(uzbl):
    uzbl.config.data.clear()
