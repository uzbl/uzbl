import logging
import asyncore
from uzbl.net import Listener, Protocol
from uzbl.core import Uzbl

logger = logging.getLogger('daemon')


class PluginDirectory(object):
    def __init__(self):
        self.global_plugins = []
        self.per_instance_plugins = []

    def load(self):
        ''' Import plugin files '''

        import uzbl.plugins
        import pkgutil

        path = uzbl.plugins.__path__
        for impr, name, ispkg in pkgutil.iter_modules(path, 'uzbl.plugins.'):
            __import__(name, globals(), locals())

        from uzbl.ext import global_registry, per_instance_registry
        self.global_plugins.extend(global_registry)
        self.per_instance_plugins.extend(per_instance_registry)


class UzblEventDaemon(object):
    def __init__(self, plugind, config, server_socket, auto_close=False,
                 print_events=False):
        self.server_socket = server_socket
        self.auto_close = auto_close
        self.print_events = print_events
        self.plugind = plugind
        self.config = config
        self._plugin_instances = []
        self._quit = False

        # Hold uzbl instances
        # {child socket: Uzbl instance, ..}
        self.uzbls = {}

        self.plugins = {}

        # Scan plugin directory for plugins
        self.plugind.load()

        # Initialise global plugins with instances in self.plugins
        self.init_plugins()

    def init_plugins(self):
        '''Initialise event manager plugins.'''
        self.plugins = {}

        for plugin in self.plugind.global_plugins:
            pinst = plugin(self)
            self._plugin_instances.append(pinst)
            self.plugins[plugin] = pinst

    def listen(self):
        '''Start listening on socket'''
        self.listener = Listener(self.server_socket)
        self.listener.target = self
        self.listener.start()

    def run(self):
        '''Main event daemon loop.'''

        logger.debug('entering main loop')

        asyncore.loop()

        # Clean up and exit
        self.quit()

        logger.debug('exiting main loop')

    def add_instance(self, sock):
        proto = Protocol(sock)
        uzbl = Uzbl(self, proto, self.print_events)
        self.uzbls[sock] = uzbl
        for plugin in self.plugins.values():
            plugin.new_uzbl(uzbl)

    def remove_instance(self, sock):
        if sock in self.uzbls:
            for plugin in self.plugins.values():
                plugin.free_uzbl(self.uzbls[sock])
            del self.uzbls[sock]
        if not self.uzbls and self.auto_close:
            self.quit()

    def close_server_socket(self):
        '''Close and delete the server socket.'''

        try:
            self.listener.close()
        except:
            logger.error('failed to close server socket', exc_info=True)

    def get_plugin_config(self, name):
        if name not in self.config:
            self.config.add_section(name)
        return self.config[name]

    def quit(self, sigint=None, *args):
        '''Close all instance socket objects, server socket and delete the
        pid file.'''

        if not self._quit:
            logger.debug('shutting down event manager')

        self.close_server_socket()

        for uzbl in list(self.uzbls.values()):
            uzbl.close()

        if not self._quit:
            for plugin in self._plugin_instances:
                plugin.cleanup()
            del self.plugins  # to avoid cyclic links
            del self._plugin_instances

        if not self._quit:
            logger.info('event manager shut down')
            self._quit = True
