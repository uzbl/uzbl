from mock import Mock
import logging
from uzbl.event_manager import Uzbl


class EventManagerMock(object):
    def __init__(self,
        global_plugins=(), instance_plugins=(),
        global_mock_plugins=(), instance_mock_plugins=(),
        plugin_config=None
    ):
        self.uzbls = {}
        self.plugins = {}
        self.instance_plugins = instance_plugins
        self.instance_mock_plugins = instance_mock_plugins
        self.plugin_config = plugin_config or {}

        for plugin in global_plugins:
            self.plugins[plugin] = plugin(self)
        for (plugin, mock) in global_mock_plugins:
            self.plugins[plugin] = mock() if mock else Mock(plugin)

    def add(self):
        u = Mock(spec=Uzbl)
        u.parent = self
        u.logger = logging.getLogger('debug')
        u.plugins = {}
        for plugin in self.instance_plugins:
            u.plugins[plugin] = plugin(u)
        for (plugin, mock) in self.instance_mock_plugins:
            u.plugins[plugin] = mock() if mock else Mock(plugin)
        self.uzbls[Mock()] = u
        return u

    def get_plugin_config(self, section):
        return self.plugin_config.get(section, {})
