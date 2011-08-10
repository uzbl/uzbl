from collections import defaultdict

import uzbl.plugins.config
from .on_set import OnSetPlugin
from .config import Config
from uzbl.arguments import splitquoted

def parse_mode_config(uzbl, args):
    '''Parse `MODE_CONFIG <mode> <var> = <value>` event and update config if
    the `<mode>` is the current mode.'''

    args = splitquoted(args)
    assert len(args) >= 3, 'missing mode config args'
    mode = args[0]
    key = args[1]
    assert args[2] == '=', 'invalid mode config set syntax'
    value = args.raw(3).strip()

    uzbl.mode_config[mode][key] = value
    config = Config[uzbl]
    if config.get('mode', None) == mode:
        config[key] = value


def default_mode_updated(uzbl, var, mode):
    config = Config[uzbl]
    if mode and not config.get('mode', None):
        logger.debug('setting mode to default %r' % mode)
        config['mode'] = mode


def mode_updated(uzbl, var, mode):
    config = Config[uzbl]
    if not mode:
        mode = config.get('default_mode', 'command')
        logger.debug('setting mode to default %r' % mode)
        config['mode'] = mode
        return

    # Load mode config
    mode_config = uzbl.mode_config.get(mode, None)
    if mode_config:
        config.update(mode_config)

    uzbl.send('event MODE_CONFIRM %s' % mode)


def confirm_change(uzbl, mode):
    config = Config[uzbl]
    if mode and config.get('mode', None) == mode:
        uzbl.event('MODE_CHANGED', mode)


# plugin init hook
def init(uzbl):

    # Usage `uzbl.mode_config[mode][key] = value`
    export(uzbl, 'mode_config', defaultdict(dict))

    connect_dict(uzbl, {
        'MODE_CONFIG':  parse_mode_config,
        'MODE_CONFIRM': confirm_change,
    })

# plugin after hook
def after(uzbl):
    OnSetPlugin[uzbl].on_set('mode', mode_updated)
    OnSetPlugin[uzbl].on_set('default_mode', default_mode_updated)

# plugin cleanup hook
def cleanup(uzbl):
    uzbl.mode_config.clear()
