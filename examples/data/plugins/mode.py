from collections import defaultdict

def parse_mode_config(uzbl, args):
    '''Parse `MODE_CONFIG <mode> <var> = <value>` event and update config if
    the `<mode>` is the current mode.'''

    ustrip = unicode.strip
    args = unicode(args)

    assert args.strip(), "missing mode config args"
    (mode, args) = map(ustrip, (args.strip().split(' ', 1) + ['',])[:2])

    assert args.strip(), "missing mode config set arg"
    (key, value) = map(ustrip, (args.strip().split('=', 1) + [None,])[:2])
    assert key and value is not None, "invalid mode config set syntax"

    uzbl.mode_config[mode][key] = value
    if uzbl.config.get('mode', None) == mode:
        uzbl.config[key] = value


def default_mode_updated(uzbl, var, mode):
    if mode and not uzbl.config.get('mode', None):
        logger.debug('setting mode to default %r' % mode)
        uzbl.config['mode'] = mode


def mode_updated(uzbl, var, mode):
    if not mode:
        mode = uzbl.config.get('default_mode', 'command')
        logger.debug('setting mode to default %r' % mode)
        uzbl.config['mode'] = mode
        return

    # Load mode config
    mode_config = uzbl.mode_config.get(mode, None)
    if mode_config:
        uzbl.config.update(mode_config)

    uzbl.send('event MODE_CONFIRM %s' % mode)


def confirm_change(uzbl, mode):
    if mode and uzbl.config.get('mode', None) == mode:
        uzbl.event('MODE_CHANGED', mode)


# plugin init hook
def init(uzbl):
    require('config')
    require('on_set')

    # Usage `uzbl.mode_config[mode][key] = value`
    export(uzbl, 'mode_config', defaultdict(dict))

    connect_dict(uzbl, {
        'MODE_CONFIG':  parse_mode_config,
        'MODE_CONFIRM': confirm_change,
    })

# plugin after hook
def after(uzbl):
    uzbl.on_set('mode', mode_updated)
    uzbl.on_set('default_mode', default_mode_updated)

# plugin cleanup hook
def cleanup(uzbl):
    uzbl.mode_config.clear()
