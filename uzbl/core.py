import time
import logging
import asynchat
from collections import defaultdict


class Protocol(asynchat.async_chat):

    def __init__(self, socket, uzbl):
        asynchat.async_chat.__init__(self, socket)
        self.uzbl = uzbl
        self.buffer = bytearray()
        self.set_terminator(b'\n')

    def collect_incoming_data(self, data):
        self.buffer += data

    def found_terminator(self):
        val = self.buffer.decode('utf-8')
        del self.buffer[:]
        self.uzbl.parse_msg(val)

    def handle_error(self):
        raise


class Uzbl(object):

    def __init__(self, parent, child_socket, options):
        self.opts = options
        self.parent = parent
        self.proto = Protocol(child_socket, self)
        self._child_socket = child_socket
        self.time = time.time()
        self.pid = None
        self.name = None

        # Flag if the instance has raised the INSTANCE_START event.
        self.instance_start = False

        # Use name "unknown" until name is discovered.
        self.logger = logging.getLogger('uzbl-instance[]')

        # Track plugin event handlers
        self.handlers = defaultdict(list)

        # Internal vars
        self._depth = 0
        self._buffer = ''

    def __repr__(self):
        return '<uzbl(%s)>' % ', '.join([
            'pid=%s' % (self.pid if self.pid else "Unknown"),
            'name=%s' % ('%r' % self.name if self.name else "Unknown"),
            'uptime=%f' % (time.time() - self.time),
            '%d handlers' % sum([len(l) for l in list(self.handlers.values())])])

    def init_plugins(self):
        '''Creates instances of per-instance plugins'''
        from uzbl.ext import per_instance_registry

        self._plugin_instances = []
        self.plugins = {}
        for plugin in per_instance_registry:
            pinst = plugin(self)
            self._plugin_instances.append(pinst)
            self.plugins[plugin] = pinst

    def send(self, msg):
        '''Send a command to the uzbl instance via the child socket
        instance.'''

        msg = msg.strip()

        if self.opts.print_events:
            print(('%s<-- %s' % ('  ' * self._depth, msg)))

        self.proto.push((msg+'\n').encode('utf-8'))

    def parse_msg(self, line):
        '''Parse an incoming message from a uzbl instance. Event strings
        will be parsed into `self.event(event, args)`.'''

        # Split by spaces (and fill missing with nulls)
        elems = (line.split(' ', 3) + [''] * 3)[:4]

        # Ignore non-event messages.
        if elems[0] != 'EVENT':
            self.logger.info('non-event message: %r', line)
            if self.opts.print_events:
                print(('--- %s' % line))
            return

        # Check event string elements
        (name, event, args) = elems[1:]
        assert name and event, 'event string missing elements'
        if not self.name:
            self.name = name
            self.logger = logging.getLogger('uzbl-instance%s' % name)
            self.logger.info('found instance name %r', name)

        assert self.name == name, 'instance name mismatch'

        # Handle the event with the event handlers through the event method
        self.event(event, args)

    def event(self, event, *args, **kargs):
        '''Raise an event.'''

        event = event.upper()

        if not self.opts.daemon_mode and self.opts.print_events:
            elems = [event]
            if args:
                elems.append(str(args))
            if kargs:
                elems.append(str(kargs))
            print(('%s--> %s' % ('  ' * self._depth, ' '.join(elems))))

        if event == "INSTANCE_START" and args:
            assert not self.instance_start, 'instance already started'

            self.pid = int(args[0])
            self.logger.info('found instance pid %r', self.pid)

            self.init_plugins()

        elif event == "INSTANCE_EXIT":
            self.logger.info('uzbl instance exit')
            self.close()

        if event not in self.handlers:
            return

        for handler in self.handlers[event]:
            self._depth += 1
            try:
                handler(*args, **kargs)

            except Exception:
                self.logger.error('error in handler', exc_info=True)

            self._depth -= 1

    def close_connection(self, child_socket):
        '''Close child socket and delete the uzbl instance created for that
        child socket connection.'''
        self.proto.close()

    def close(self):
        '''Close the client socket and call the plugin cleanup hooks.'''

        self.logger.debug('called close method')

        # Remove self from parent uzbls dict.
        self.logger.debug('removing self from uzbls list')
        self.parent.remove_instance(self._child_socket)

        for plugin in self._plugin_instances:
            plugin.cleanup()
        del self.plugins  # to avoid cyclic links
        del self._plugin_instances

        self.logger.info('removed %r', self)

    def connect(self, name, handler):
        """Attach event handler

        No extra arguments added. Use bound methods and partials to have
        extra arguments.
        """
        self.handlers[name].append(handler)

