from .event_manager import Uzbl
from six import add_metaclass
import logging


per_instance_registry = []
global_registry = []


class PluginMeta(type):
    """Registers plugin in registry so that it instantiates when needed"""

    def __init__(self, name, bases, dic):
        super(PluginMeta, self).__init__(name, bases, dic)
        # Sorry, a bit of black magick
        if bases == (object,) or bases == (BasePlugin,):
            # base classes for the plugins
            return
        if issubclass(self, PerInstancePlugin):
            per_instance_registry.append(self)
        elif issubclass(self, GlobalPlugin):
            global_registry.append(self)

    def __getitem__(self, owner):
        """This method returns instance of plugin corresponding to owner

        :param owner: can be uzbl or event manager

        If you will try to get instance of :class:`GlobalPlugin` on uzbl
        instance it will find instance on it's parent. If you will try to
        find instance of a :class:`PerInstancePlugin` it will raise
        :class:`ValueError`
        """
        return self._get_instance(owner)


@add_metaclass(PluginMeta)
class BasePlugin(object):
    """Base class for all uzbl plugins"""


class PerInstancePlugin(BasePlugin):
    """Base class for plugins which instantiate once per uzbl instance"""

    def __init__(self, uzbl):
        self.uzbl = uzbl
        self.plugin_config = uzbl.parent.get_plugin_config(self.CONFIG_SECTION)
        self.logger = uzbl.logger  # we can also append plugin name to logger

    def cleanup(self):
        """Cleanup state after instance is gone

        Default function avoids cyclic refrences, so don't forget to call
        super() if overriding
        """
        del self.uzbl

    @classmethod
    def _get_instance(cls, owner):
        """Returns instance of the plugin

        This method should be private to not violate TOOWTDI
        """
        if not isinstance(owner, Uzbl):
            raise ValueError("Can only get {0} instance for uzbl, not {1}"
                .format(cls.__name__, type(owner).__name__))
        # TODO(tailhook) probably subclasses can be returned as well
        return owner.plugins[cls]


class GlobalPlugin(BasePlugin):
    """Base class for plugins which instantiate once per daemon"""

    def __init__(self, event_manager):
        self.event_manager = event_manager
        self.plugin_config = event_manager.get_plugin_config(self.CONFIG_SECTION)
        self.logger = logging.getLogger(self.__module__)

    def new_uzbl(self, uzbl):
        pass

    def free_uzbl(self, uzbl):
        pass

    @classmethod
    def _get_instance(cls, owner):
        """Returns instance of the plugin

        This method should be private to not violate TOOWTDI
        """
        if isinstance(owner, Uzbl):
            owner = owner.parent
        # TODO(tailhook) probably subclasses can be returned as well
        return owner.plugins[cls]

    def cleanup(self):
        """Cleanup state after instance is gone

        Default function avoids cyclic refrences, so don't forget to call
        super() if overriding
        """
        del self.event_manager
