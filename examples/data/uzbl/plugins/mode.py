import sys
import re

__export__ = ['set_mode', 'get_mode']

UZBLS = {}

DEFAULTS = {
  'mode': '',
  'modes': {
    'insert': {
      'forward_keys': True,
      'keycmd_events': False,
      'modcmd_updates': False,
      'indicator': 'I'},
    'command': {
      'forward_keys': False,
      'keycmd_events': True,
      'modcmd_updates': True,
      'indicator': 'C'}}}

FINDSPACES = re.compile("\s+")
VALID_KEY = re.compile("^[\w_]+$").match

def error(msg):
    sys.stderr.write("mode plugin: error: %s\n" % msg)


def add_instance(uzbl, *args):
    UZBLS[uzbl] = dict(DEFAULTS)


def del_instance(uzbl, *args):
    if uzbl in UZBLS:
        del UZBLS[uzbl]


def get_mode_dict(uzbl):
    if uzbl not in UZBLS:
        add_instance(uzbl)

    return UZBLS[uzbl]


def get_mode_config(uzbl, mode):
    modes = get_mode_dict(uzbl)['modes']
    if mode not in modes:
        modes[mode] = {}

    return modes[mode]


def get_mode(uzbl):
    return get_mode_dict(uzbl)['mode']


def set_mode(uzbl, mode=None):
    mode_dict = get_mode_dict(uzbl)
    config = uzbl.get_config()

    if mode is None:
        if 'default_mode' not in config:
            return

        mode = config['default_mode']

    if not VALID_KEY(mode):
        raise KeyError("invalid mode name: %r" % mode)

    if 'mode' not in config or config['mode'] != mode:
        config['mode'] = mode
        return

    mode_dict['mode'] = mode
    mode_config = get_mode_config(uzbl, mode)

    for (key, value) in mode_config.items():
        if key not in config:
            config[key] = value

        elif config[key] != value:
            config[key] = value

    if 'mode_indicator' not in mode_config:
        config['mode_indicator'] = mode

    uzbl.clear_keycmd()
    uzbl.event("MODE_CHANGED", mode)


def config_changed(uzbl, key, value):
    value = None if not value else value
    if key == 'default_mode':
        if not get_mode(uzbl):
            set_mode(uzbl, value)

    elif key == 'mode':
        set_mode(uzbl, value)


def mode_config(uzbl, args):

    split = map(unicode.strip, FINDSPACES.split(args.lstrip(), 1))
    if len(split) != 2:
        raise SyntaxError('invalid config syntax: %r' % args)

    mode, set = split
    split = map(unicode.strip, set.split('=', 1))
    if len(split) != 2:
        raise SyntaxError('invalid set syntax: %r' % args)

    key, value = split
    if not VALID_KEY(key):
        raise KeyError('invalid config key: %r' % key)

    mode_config = get_mode_config(uzbl, mode)
    mode_config[key] = value

    if get_mode(uzbl) == mode:
        uzbl.set(key, value)


def load_reset(uzbl, *args):
    config = uzbl.get_config()
    if 'reset_on_commit' not in config or config['reset_on_commit'] == '1':
        set_mode(uzbl)


def toggle_modes(uzbl, modes):

    modelist = [s.strip() for s in modes.split(' ') if s]
    if not len(modelist):
        return error("no modes specified to toggle")

    mode_dict = get_mode_dict(uzbl)
    oldmode = mode_dict['mode']
    if oldmode not in modelist:
        return set_mode(uzbl, modelist[0])

    newmode = modelist[(modelist.index(oldmode)+1) % len(modelist)]
    set_mode(uzbl, newmode)


def init(uzbl):

    connects = {'CONFIG_CHANGED': config_changed,
      'INSTANCE_EXIT': del_instance,
      'INSTANCE_START': add_instance,
      'MODE_CONFIG': mode_config,
      'LOAD_START': load_reset,
      'TOGGLE_MODES': toggle_modes}

    uzbl.connect_dict(connects)
