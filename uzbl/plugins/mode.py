from collections import defaultdict

import uzbl.plugins.config
from .on_set import OnSetPlugin
from .config import Config
from uzbl.arguments import splitquoted
from uzbl.ext import PerInstancePlugin


class ModePlugin(PerInstancePlugin):
    def __init__(self, uzbl):
        super(ModePlugin, self).__init__(uzbl)
        self.mode_config = defaultdict(dict)
        uzbl.connect('MODE_CONFIG', self.parse_mode_config)
        uzbl.connect('MODE_CONFIRM', self.confirm_change)
        OnSetPlugin[uzbl].on_set('mode', self.mode_updated, False)
        OnSetPlugin[uzbl].on_set('default_mode', self.default_mode_updated, False)

    def cleanup(self):
        self.mode_config.clear()

    def parse_mode_config(self, args):
        '''Parse `MODE_CONFIG <mode> <var> = <value>` event and update config
        if the `<mode>` is the current mode.'''

        args = splitquoted(args)
        assert len(args) >= 3, 'missing mode config args'
        mode = args[0]
        key = args[1]
        assert args[2] == '=', 'invalid mode config set syntax'
        value = args.raw(3).strip()

        self.mode_config[mode][key] = value
        config = Config[self.uzbl]
        if config.get('mode', None) == mode:
            config[key] = value

    def default_mode_updated(self, var, mode):
        config = Config[self.uzbl]
        if mode and not config.get('mode', None):
            logger.debug('setting mode to default %r' % mode)
            config['mode'] = mode

    def mode_updated(self, var, mode):
        config = Config[self.uzbl]
        if not mode:
            mode = config.get('default_mode', 'command')
            logger.debug('setting mode to default %r' % mode)
            config['mode'] = mode
            return

        # Load mode config
        mode_config = self.mode_config.get(mode, None)
        if mode_config:
            config.update(mode_config)

        self.uzbl.send('event MODE_CONFIRM %s' % mode)

    def confirm_change(self, mode):
        config = Config[self.uzbl]
        if mode and config.get('mode', None) == mode:
            self.uzbl.event('MODE_CHANGED', mode)

