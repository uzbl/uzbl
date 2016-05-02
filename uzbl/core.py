import time
import logging
from collections import defaultdict


class Uzbl(object):

    def __init__(self, parent, proto, print_events=False):
        proto.target = self
        self.print_events = print_events
        self.parent = parent
        self.proto = proto
        self.time = time.time()
        self.pid = None
        self.name = None

        # Flag if the instance has raised the INSTANCE_START event.
        self.instance_start = False

        # Use name "unknown" until name is discovered.
        self.logger = logging.getLogger('uzbl-instance[]')

        # Plugin instances
        self._plugin_instances = []
        self.plugins = {}

        # Track plugin event handlers
        self.handlers = defaultdict(list)
        self.request_handlers = defaultdict(list)

        # Internal vars
        self._depth = 0
        self._buffer = ''

    def __repr__(self):
        return '<uzbl(%s)>' % ', '.join([
            'pid=%s' % (self.pid if self.pid else "Unknown"),
            'name=%s' % ('%r' % self.name if self.name else "Unknown"),
            'uptime=%f' % (time.time() - self.time),
            '%d handlers' % sum([len(l) for l in list(self.handlers.values())]),
            '%d request handlers' % sum([len(l) for l in list(self.request_handlers.values())])])

    def init_plugins(self):
        '''Creates instances of per-instance plugins'''

        for plugin in self.parent.plugind.per_instance_plugins:
            pinst = plugin(self)
            self._plugin_instances.append(pinst)
            self.plugins[plugin] = pinst

    def send(self, msg):
        '''Send a command to the uzbl instance via the child socket
        instance.'''

        msg = msg.strip()

        if self.print_events:
            self.logger.debug(('%s<-- %s' % ('  ' * self._depth, msg)))

        self.proto.push((msg+'\n').encode('utf-8'))

    def reply(self, cookie, response):
        if self.print_events:
            self.logger.debug(('%s<?- %s %s' % ('  ' * self._depth, cookie, response)))

        self.proto.push(('REPLY-%s %s\n' % (cookie, response)).encode('utf-8'))

    def parse_msg(self, line):
        '''Parse an incoming message from a uzbl instance. Event strings
        will be parsed into `self.event(event, args)`.'''

        # Split by spaces (and fill missing with nulls)
        elems = (line.split(' ', 3) + [''] * 3)[:4]

        handler = None
        kargs = {}

        # Ignore non-event messages.
        if elems[0].startswith('REQUEST-'):
            handler = self.request
            kargs['cookie'] = elems[0][8:]
        elif elems[0] == 'EVENT':
            handler = self.event

        if handler is None:
            if line:
                self.logger.info('unrecognized message: %r', line)
                if self.print_events:
                    self.logger.debug(('--- %s' % line))
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
        handler(event, args, **kargs)

    def request(self, request, *args, **kargs):
        '''Complete a request.'''

        if 'cookie' not in kargs:
            self.logger.info('Found a request without a cookie: %r', request)
            return

        cookie = kargs['cookie']

        request = request.upper()

        if self.print_events:
            elems = [request]
            if args:
                elems.append(str(args))
            if kargs:
                elems.append(str(kargs))
            self.logger.debug(('%s-?> %s %s' % ('  ' * self._depth, cookie, ' '.join(elems))))

        final_response = None

        if request in self.request_handlers:
            for (prio, handler) in self.request_handlers[request]:
                self._depth += 1
                try:
                    (response, args, kargs) = handler(final_response, *args, **kargs)
                    if response is not None:
                        final_response = response
                except BaseException:
                    self.logger.error('error in request handler for \'%s\'', request, exc_info=True)
                self._depth -= 1

        if final_response is None:
            final_response = ''

        self.reply(cookie, final_response)

    def event(self, event, *args, **kargs):
        '''Raise an event.'''

        event = event.upper()

        if self.print_events:
            elems = [event]
            if args:
                elems.append(str(args))
            if kargs:
                elems.append(str(kargs))
            self.logger.debug(('%s--> %s' % ('  ' * self._depth, ' '.join(elems))))

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

            except BaseException:
                self.logger.error('error in handler for \'%s\'', event, exc_info=True)

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
        self.parent.remove_instance(self.proto.socket)

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

    def answer_request(self, name, prio, handler):
        """Attach request handler

        No extra arguments added. Use bound methods and partials to have
        extra arguments.
        """

        def fst(a):
            return a[0]

        self.request_handlers[name].append((prio, handler))
        self.request_handlers[name].sort(key=fst)
