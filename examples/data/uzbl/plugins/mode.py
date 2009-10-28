import sys
import re

__export__ = ['set_mode', 'get_mode']

UZBLS = {}

DEFAULTS = {
  'mode': '',
  'default': '',
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

_RE_FINDSPACES = re.compile("\s+")


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


def key_press(uzbl, key):
    if key != "Escape":
        return

    set_mode(uzbl)


def set_mode(uzbl, mode=None):
    mode_dict = get_mode_dict(uzbl)
    if mode is None:
        if not mode_dict['default']:
            return error("no default mode to fallback on")

        mode = mode_dict['default']

    config = uzbl.get_config()
    if 'mode' not in config or config['mode'] != mode:
        config['mode'] = mode

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
    if key == 'default_mode':
        mode_dict = get_mode_dict(uzbl)
        mode_dict['default'] = value
        if value and not mode_dict['mode']:
            set_mode(uzbl, value)

    elif key == 'mode':
        if not value:
            value = None

        set_mode(uzbl, value)


def mode_config(uzbl, args):

    split = map(unicode.strip, _RE_FINDSPACES.split(args.lstrip(), 1))
    if len(split) != 2:
        return error("invalid MODE_CONFIG syntax: %r" % args)

    mode, set = split
    split = map(unicode.strip, set.split('=', 1))
    if len(split) != 2:
        return error("invalid MODE_CONFIG set command: %r" % args)

    key, value = split
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
      'KEY_PRESS': key_press,
      'MODE_CONFIG': mode_config,
      'LOAD_START': load_reset,
      'TOGGLE_MODES': toggle_modes}

    uzbl.connect_dict(connects)
