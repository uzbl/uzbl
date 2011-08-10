from re import compile

from uzbl.arguments import splitquoted
from uzbl.ext import PerInstancePlugin

types = {'int': int, 'float': float, 'str': unicode}

valid_key = compile('^[A-Za-z0-9_\.]+$').match

class Config(PerInstancePlugin):
    """Configuration plugin, has dictionary interface for config access

    This class is currenty not inherited from either UserDict or abc.Mapping
    because not sure what version of python we want to support. It's not
    hard to implement all needed methods either.
    """

    def __init__(self, uzbl):
        super(Config, self).__init__(uzbl)

        self.data = {}
        uzbl.connect('VARIABLE_SET', self.parse_set_event)
        assert not 'a' in self.data

    def __getitem__(self, key):
        return self.data[key]

    def __setitem__(self, key, value):
        self.set(key, value)

    def __delitem__(self, key):
        self.set(key)

    def get(self, key, default=None):
        return self.data.get(key, default)

    def __contains__(self, key):
        return key in self.data

    def keys(self):
        return self.data.iterkeys()

    def items(self):
        return self.data.iteritems()

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

        if isinstance(value, bool):
            value = int(value)

        else:
            value = unicode(value)
            assert '\n' not in value

        if not force and key in self and self[key] == value:
            return

        self.uzbl.send(u'set %s = %s' % (key, value))


    def parse_set_event(self, args):
        '''Parse `VARIABLE_SET <var> <type> <value>` event and load the
        (key, value) pair into the `uzbl.config` dict.'''

        args = splitquoted(args)
        if len(args) == 2:
            key, type, raw_value = args[0], args[1], ''
        elif len(args) == 3:
            key, type, raw_value = args
        else:
            raise Exception('Invalid number of arguments')

        assert valid_key(key)
        assert type in types

        new_value = types[type](raw_value)
        old_value = self.data.get(key, None)

        # Update new value.
        self.data[key] = new_value

        if old_value != new_value:
            self.uzbl.event('CONFIG_CHANGED', key, new_value)

        # Cleanup null config values.
        if type == 'str' and not new_value:
            del self.data[key]

    def cleanup(self):
        # not sure it's needed, but safer for cyclic links
        self.data.clear()
        super(Config, self).cleanup()

