import sys
import re

__export__ = ['set_mode', 'get_mode', 'set_mode_config', 'get_mode_config']

UZBLS = {}

DEFAULTS = {
  'mode': '',
  'modes': {
    'insert': {
      'forward_keys': True,
      'keycmd_events': False,
      'modcmd_updates': False,
      'mode_indicator': 'I'},
    'command': {
      'forward_keys': False,
      'keycmd_events': True,
      'modcmd_updates': True,
      'mode_indicator': 'C'}}}

FINDSPACES = re.compile("\s+")
VALID_KEY = re.compile("^[\w_]+$").match


def add_instance(uzbl, *args):
    UZBLS[uzbl] = dict(DEFAULTS)


def del_instance(uzbl, *args):
    if uzbl in UZBLS:
        del UZBLS[uzbl]


def get_mode_dict(uzbl):
    '''Return the mode dict for an instance.'''

    if uzbl not in UZBLS:
        add_instance(uzbl)

    return UZBLS[uzbl]


def get_mode_config(uzbl, mode):
    '''Return the mode config for a given mode.'''

    modes = get_mode_dict(uzbl)['modes']
    if mode not in modes:
        modes[mode] = {}

    return modes[mode]


def get_mode(uzbl):
    return get_mode_dict(uzbl)['mode']


def mode_changed(uzbl, mode):
    '''The mode has just been changed, now set the per-mode config.'''

    if get_mode(uzbl) != mode:
        return

    config = uzbl.get_config()
    mode_config = get_mode_config(uzbl, mode)
    for (key, value) in mode_config.items():
        uzbl.set(key, value, config=config)

    if 'mode_indicator' not in mode_config:
        config['mode_indicator'] = mode

    uzbl.clear_keycmd()
    uzbl.clear_modcmd()


def set_mode(uzbl, mode=None):
    '''Set the mode and raise the MODE_CHANGED event if the mode has changed.
    Fallback on the default mode if no mode argument was given and the default
    mode is not null.'''

    config = uzbl.get_config()
    mode_dict = get_mode_dict(uzbl)
    if mode is None:
        mode_dict['mode'] = ''
        if 'default_mode' in config:
            mode = config['default_mode']

        else:
            mode = 'command'

    if not VALID_KEY(mode):
        raise KeyError("invalid mode name: %r" % mode)

    if 'mode' not in config or config['mode'] != mode:
        config['mode'] = mode

    elif mode_dict['mode'] != mode:
        mode_dict['mode'] = mode
        uzbl.event("MODE_CHANGED", mode)


def config_changed(uzbl, key, value):
    '''Check for mode related config changes.'''

    value = None if not value else value
    if key == 'default_mode':
        if not get_mode(uzbl):
            set_mode(uzbl, value)

    elif key == 'mode':
        set_mode(uzbl, value)


def set_mode_config(uzbl, mode, key, value):
    '''Set mode specific configs. If the mode being modified is the current
    mode then apply the changes on the go.'''

    assert VALID_KEY(mode) and VALID_KEY(key)

    mode_config = get_mode_config(uzbl, mode)
    mode_config[key] = value

    if get_mode(uzbl) == mode:
        uzbl.set(key, value)


def mode_config(uzbl, args):
    '''Parse mode config events.'''

    split = map(unicode.strip, FINDSPACES.split(args.lstrip(), 1))
    if len(split) != 2:
        raise SyntaxError('invalid mode config syntax: %r' % args)

    mode, set = split
    split = map(unicode.strip, set.split('=', 1))
    if len(split) != 2:
        raise SyntaxError('invalid set syntax: %r' % args)

    key, value = split
    set_mode_config(uzbl, mode, key, value)


def toggle_modes(uzbl, modes):
    '''Toggle or cycle between or through a list of modes.'''

    assert len(modes.strip())

    modelist = filter(None, map(unicode.strip, modes.split(' ')))
    mode = get_mode(uzbl)

    index = 0
    if mode in modelist:
        index = (modelist.index(mode)+1) % len(modelist)

    set_mode(uzbl, modelist[index])


def init(uzbl):
    # Event handling hooks.
    uzbl.connect_dict({
        'CONFIG_CHANGED':   config_changed,
        'INSTANCE_EXIT':    del_instance,
        'INSTANCE_START':   add_instance,
        'MODE_CHANGED':     mode_changed,
        'MODE_CONFIG':      mode_config,
        'TOGGLE_MODES':     toggle_modes,
    })

    # Function exports to the uzbl object, `function(uzbl, *args, ..)`
    # becomes `uzbl.function(*args, ..)`.
    uzbl.export_dict({
        'get_mode': get_mode,
        'get_mode_config': get_mode_config,
        'set_mode': set_mode,
        'set_mode_config': set_mode_config,
    })
